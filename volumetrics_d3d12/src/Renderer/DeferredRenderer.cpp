#include "pch.h"
#include "DeferredRenderer.h"

#include "D3DGraphicsContext.h"
#include "imgui.h"

#include "Application/Scene.h"

#include "Lighting/LightManager.h"
#include "Lighting/IBL.h"
#include "Lighting/Material.h"

#include "pix3.h"
#include "Framework/GuiHelpers.h"
#include "Lighting/ShadowMap.h"


// Root signatures
namespace GeometryPassRootSignature
{
	enum Parameters
	{
		ObjectConstantBuffer = 0,
		PassConstantBuffer,
		MaterialBuffer,
		Count
	};
}

namespace DepthPassRootSignature
{
	enum Parameters
	{
		ObjectConstantBuffer = 0,
		LightConstantBuffer,
		Count
	};
}

namespace SkyboxPassRootSignature
{
	enum Parameters
	{
		PassConstantBuffer = 0, // VS only
		QuadProperties,			// VS only
		EnvironmentMap,			// PS only
		EnvironmentMapSampler,	// PS only
		Count
	};
}

namespace LightingPassRootSignature
{
	enum Parameters
	{
		PassConstantBuffer = 0,
		GBuffer,				// GBuffer SRVs are sequential
		DepthBuffer,
		LightingConstantBuffer,	// Constant buffer with universal lighting parameters
		PointLightBuffer,		// A buffer of all point lights in the scene
		EnvironmentMaps,		// Environment maps are sequential
		EnvironmentSamplers,	// Environment samplers are sequential
		SunShadowMap,			// Shadow map for the sun
		ShadowMapSampler,
		OutputResource,
		Count
	};
}

namespace TonemappingRootSignature
{
	enum Parameters
	{
		TonemappingParametersConstantBuffer = 0,
		OutputResource,
		Count
	};
}

namespace ESMDownsampleTransformRootSignature
{
	enum Value
	{
		ParamsCB = 0,
		InShadowMap,
		OutESM,
		Count
	};
}


namespace SeparableBoxFilterRootSignature
{
	enum Value
	{
		ParamsCB = 0,
		InTexture,
		OutTexture,
		Count
	};
}


DeferredRenderer::DeferredRenderer()
{
	CreatePipelines();
	CreateResolutionDependentResources();

	m_PrevGBufferWidth = m_GBufferWidth;
	m_PrevGBufferHeight = m_GBufferHeight;
}

DeferredRenderer::~DeferredRenderer()
{
	// Free descriptors
	m_RTVs.Free();
	m_DSVs.Free();
	m_SRVs.Free();

	m_OutputUAV.Free();
	m_OutputRTV.Free();
}

void DeferredRenderer::SetScene(const Scene& scene, LightManager& lightManager, const MaterialManager& materialManager)
{
	m_Scene = &scene;
	m_LightManager = &lightManager;
	m_MaterialManager = &materialManager;

	m_VolumeRenderer = std::make_unique<VolumetricRendering>(*m_LightManager);
}

void DeferredRenderer::OnBackBufferResize()
{
	m_RTVs.Free();
	m_DSVs.Free();
	m_SRVs.Free();

	m_OutputUAV.Free();
	m_OutputRTV.Free();

	CreateResolutionDependentResources();
}

void DeferredRenderer::CreatePipelines()
{
	// Create geometry pass pipeline state
	{
		D3DGraphicsPipelineDesc psoDesc = {};

		// Set up root parameters
		CD3DX12_ROOT_PARAMETER1 rootParameters[GeometryPassRootSignature::Count];
		rootParameters[GeometryPassRootSignature::ObjectConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[GeometryPassRootSignature::PassConstantBuffer].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[GeometryPassRootSignature::MaterialBuffer].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and deny unnecessary access to certain pipeline stages
		psoDesc.NumRootParameters = GeometryPassRootSignature::Count;
		psoDesc.RootParameters = rootParameters;
		psoDesc.RootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;

		for (const auto& element : VertexType::InputLayout)
		{
			psoDesc.InputLayout.push_back(element);
		}

		// Shaders
		psoDesc.VertexShader.ShaderPath = L"assets/shaders/gbuffer/geometry_pass_vs.hlsl";
		psoDesc.VertexShader.EntryPoint = L"main";

		psoDesc.PixelShader.ShaderPath = L"assets/shaders/gbuffer/geometry_pass_ps.hlsl";
		psoDesc.PixelShader.EntryPoint = L"main";

		// Add all render target formats to the description
		psoDesc.RenderTargetFormats.insert(psoDesc.RenderTargetFormats.end(), m_RTFormats.begin(), m_RTFormats.end());
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		m_GeometryPassPipeline.Create(&psoDesc);
	}

	// Create depth only pipeline state
	{
		D3DGraphicsPipelineDesc psoDesc = {};

		CD3DX12_ROOT_PARAMETER1 rootParameters[DepthPassRootSignature::Count];
		rootParameters[DepthPassRootSignature::ObjectConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[DepthPassRootSignature::LightConstantBuffer].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

		psoDesc.NumRootParameters = DepthPassRootSignature::Count;
		psoDesc.RootParameters = rootParameters;
		psoDesc.RootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;

		for (const auto& element : VertexType::InputLayout)
		{
			psoDesc.InputLayout.push_back(element);
		}

		// Shaders
		psoDesc.VertexShader.ShaderPath = L"assets/shaders/gbuffer/depth_pass_vs.hlsl";
		psoDesc.VertexShader.EntryPoint = L"main";

		psoDesc.PixelShader.ShaderPath = nullptr;
		psoDesc.PixelShader.EntryPoint = nullptr;

		psoDesc.DSVFormat = ShadowMap::GetDSVFormat();

		psoDesc.RasterizerDesc.DepthBias = 10'000;
		psoDesc.RasterizerDesc.DepthBiasClamp = 0.01f;
		psoDesc.RasterizerDesc.SlopeScaledDepthBias = 2.5f;

		m_DepthOnlyPipeline.Create(&psoDesc);
	}

	// Create skybox pipeline state
	{
		D3DGraphicsPipelineDesc psoDesc = {};

		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[SkyboxPassRootSignature::Count];
		rootParameters[SkyboxPassRootSignature::PassConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[SkyboxPassRootSignature::QuadProperties].InitAsConstants(SizeOfInUint32(FullscreenQuadProperties), 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[SkyboxPassRootSignature::EnvironmentMap].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[SkyboxPassRootSignature::EnvironmentMapSampler].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

		// Deny unnecessary access to certain pipeline stages
		psoDesc.NumRootParameters = SkyboxPassRootSignature::Count;
		psoDesc.RootParameters = rootParameters;
		psoDesc.RootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;

		// Shaders
		psoDesc.VertexShader.ShaderPath = L"assets/shaders/fullscreen_vs.hlsl";
		psoDesc.VertexShader.EntryPoint = L"main";

		psoDesc.PixelShader.ShaderPath = L"assets/shaders/gbuffer/skybox_ps.hlsl";
		psoDesc.PixelShader.EntryPoint = L"main";

		// Only a single render target will be used for the skybox pass
		// It will be the same format as the output resource of the deferred renderer
		psoDesc.RenderTargetFormats.push_back(m_OutputFormat);
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// Only draw skybox where depth is 1.0f
		psoDesc.DepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

		m_SkyboxPipeline.Create(&psoDesc);
	}

	// Create lighting pass pipeline state
	{
		D3DComputePipelineDesc psoDesc = {};

		CD3DX12_DESCRIPTOR_RANGE1 ranges[7];

		// All descriptors in the g-buffer are contiguous
		// There will be one for each render target plus the depth buffer
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, s_RTCount, 0, 0);

		// Depth buffer
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);

		// Environment lighting descriptors
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1);

		// Environment samplers
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0, 0);

		// Shadow map
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 2);

		ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 2, 0);

		// Output UAV
		ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);


		// Set up root parameters
		CD3DX12_ROOT_PARAMETER1 rootParameters[LightingPassRootSignature::Count];
		rootParameters[LightingPassRootSignature::PassConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
		rootParameters[LightingPassRootSignature::GBuffer].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[LightingPassRootSignature::DepthBuffer].InitAsDescriptorTable(1, &ranges[1]);
		rootParameters[LightingPassRootSignature::LightingConstantBuffer].InitAsConstantBufferView(1, 0);
		rootParameters[LightingPassRootSignature::PointLightBuffer].InitAsShaderResourceView(4, 0);
		rootParameters[LightingPassRootSignature::EnvironmentMaps].InitAsDescriptorTable(1, &ranges[2]);
		rootParameters[LightingPassRootSignature::EnvironmentSamplers].InitAsDescriptorTable(1, &ranges[3]);
		rootParameters[LightingPassRootSignature::SunShadowMap].InitAsDescriptorTable(1, &ranges[4]);
		rootParameters[LightingPassRootSignature::ShadowMapSampler].InitAsDescriptorTable(1, &ranges[5]);
		rootParameters[LightingPassRootSignature::OutputResource].InitAsDescriptorTable(1, &ranges[6]);

		psoDesc.NumRootParameters = LightingPassRootSignature::Count;
		psoDesc.RootParameters = rootParameters;

		psoDesc.Shader = L"assets/shaders/gbuffer/lighting_pass_cs.hlsl";
		psoDesc.EntryPoint = L"main";

		m_LightingPipeline.Create(&psoDesc);
	}

	// Create tonemapping pipeline
	{
		D3DComputePipelineDesc psoDesc = {};

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		// Set up root parameters
		CD3DX12_ROOT_PARAMETER1 rootParameters[TonemappingRootSignature::Count];
		rootParameters[TonemappingRootSignature::TonemappingParametersConstantBuffer].InitAsConstants(SizeOfInUint32(TonemappingParametersConstantBuffer), 0);
		rootParameters[TonemappingRootSignature::OutputResource].InitAsDescriptorTable(1, &ranges[0]);

		psoDesc.NumRootParameters = TonemappingRootSignature::Count;
		psoDesc.RootParameters = rootParameters;

		psoDesc.Shader = L"assets/shaders/compute/postprocess/tonemapping_cs.hlsl";
		psoDesc.EntryPoint = L"main";

		m_TonemappingPipeline.Create(&psoDesc);
	}

	// Create ESM downsample pipeline
	{
		D3DComputePipelineDesc psoDesc = {};

		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[ESMDownsampleTransformRootSignature::Count];
		rootParams[ESMDownsampleTransformRootSignature::ParamsCB].InitAsConstants(SizeOfInUint32(ExponentialShadowMapCB), 0);
		rootParams[ESMDownsampleTransformRootSignature::InShadowMap].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[ESMDownsampleTransformRootSignature::OutESM].InitAsDescriptorTable(1, &ranges[1]);

		psoDesc.NumRootParameters = ARRAYSIZE(rootParams);
		psoDesc.RootParameters = rootParams;

		psoDesc.Shader = L"assets/shaders/compute/esm/downsample_transform_cs.hlsl";
		psoDesc.EntryPoint = L"main";

		{
			psoDesc.StaticSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(
				0,
				D3D12_FILTER_MIN_MAG_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP
			));
		}

		m_ESMDownsamplePipeline.Create(&psoDesc);
	}

	// Separable box filter
	{
		D3DComputePipelineDesc psoDesc = {};

		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[SeparableBoxFilterRootSignature::Count];
		rootParams[SeparableBoxFilterRootSignature::ParamsCB].InitAsConstants(SizeOfInUint32(BoxFilterParamsCB), 0);
		rootParams[SeparableBoxFilterRootSignature::InTexture].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[SeparableBoxFilterRootSignature::OutTexture].InitAsDescriptorTable(1, &ranges[1]);

		psoDesc.NumRootParameters = ARRAYSIZE(rootParams);
		psoDesc.RootParameters = rootParams;

		psoDesc.Shader = L"assets/shaders/compute/filters/separable_box_filter_cs.hlsl";
		psoDesc.EntryPoint = L"HorizontalPass";

		m_SeparableBoxFilterPipelines.at(0).Create(&psoDesc);

		psoDesc.EntryPoint = L"VerticalPass";

		m_SeparableBoxFilterPipelines.at(1).Create(&psoDesc);
	}
}

void DeferredRenderer::CreateResolutionDependentResources()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	m_GBufferWidth = g_D3DGraphicsContext->GetClientWidth();
	m_GBufferHeight = g_D3DGraphicsContext->GetClientHeight();

	// Allocate render targets
	D3D12_RESOURCE_DESC desc;
	D3D12_CLEAR_VALUE clearValue;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = m_GBufferWidth;
	desc.Height = m_GBufferHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;

	for (UINT rt = 0; rt < s_RTCount; rt++)
	{
		desc.Format = m_RTFormats[rt];
		clearValue.Format = m_RTFormats[rt];
		m_RenderTargets[rt].Allocate(&desc, D3D12_RESOURCE_STATE_RENDER_TARGET, CREATE_INDEXED_NAME(L"GBuffer", rt), &clearValue);
	}

	// Allocate depth buffer
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Format = m_DSVFormat;

	clearValue.Format = m_DSVFormat;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	{
		UINT i = 0;
		for (auto& depthBuffer : m_DepthBuffers)
			depthBuffer.Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, CREATE_INDEXED_NAME(L"DepthBuffer", i++), &clearValue);
	}

	// Allocate output resource
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Format = m_OutputFormat;
	m_OutputResource.Allocate(&desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"DeferredOutput");

	// Create RTVs and DSVs
	m_RTVs = g_D3DGraphicsContext->GetRTVHeap()->Allocate(s_RTCount);
	ASSERT(m_RTVs.IsValid(), "Failed to alloc!");

	m_OutputRTV = g_D3DGraphicsContext->GetRTVHeap()->Allocate(1);
	ASSERT(m_OutputRTV.IsValid(), "Failed to alloc!");

	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			rtvDesc.Format = m_RTFormats[rt];
			device->CreateRenderTargetView(m_RenderTargets.at(rt).GetResource(), &rtvDesc, m_RTVs.GetCPUHandle(rt));
		}

		rtvDesc.Format = m_OutputFormat;
		device->CreateRenderTargetView(m_OutputResource.GetResource(), &rtvDesc, m_OutputRTV.GetCPUHandle());
	}

	m_DSVs = g_D3DGraphicsContext->GetDSVHeap()->Allocate(static_cast<UINT>(m_DepthBuffers.size()));
	ASSERT(m_DSVs.IsValid(), "Failed to alloc!");

	for (UINT i = 0; i < static_cast<UINT>(m_DepthBuffers.size()); i++)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_DSVFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(m_DepthBuffers.at(i).GetResource(), &dsvDesc, m_DSVs.GetCPUHandle(i));
	}

	// Create SRVs
	m_SRVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(GB_SRV_MAX);
	ASSERT(m_SRVs.IsValid(), "Failed to alloc!");

	{
		// Create SRVs for render targets
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			srvDesc.Format = m_RTFormats.at(rt);
			device->CreateShaderResourceView(m_RenderTargets.at(rt).GetResource(), &srvDesc, m_SRVs.GetCPUHandle(rt));
		}

		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		device->CreateShaderResourceView(m_DepthBuffers.at(0).GetResource(), &srvDesc, m_SRVs.GetCPUHandle(GB_SRV_Depth0));
		device->CreateShaderResourceView(m_DepthBuffers.at(1).GetResource(), &srvDesc, m_SRVs.GetCPUHandle(GB_SRV_Depth1));
	}

	// Allocate UAV
	m_OutputUAV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
	ASSERT(m_OutputUAV.IsValid(), "Failed to alloc");
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = m_OutputFormat;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;

		device->CreateUnorderedAccessView(m_OutputResource.GetResource(), nullptr, &uavDesc, m_OutputUAV.GetCPUHandle());
	}
}


void DeferredRenderer::Render()
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	auto FlushBarriers = [&]()
	{
		commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		barriers.clear();
	};
	auto AddTransition = [&](const auto& resource, auto before, auto after)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource.GetResource(), before, after));
	};

	// Render shadow maps first
	RenderShadowMaps();

	g_D3DGraphicsContext->RestoreDefaultViewport();

	{
		AddTransition(GetCurrentDepthBuffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		FlushBarriers();
	}

	// Then render geometry pass
	ClearGBuffer();

	GeometryPass();

	// After copying to the back-buffer, the output resource will be returned to unordered access state
	// It needs to be transitioned to a render target
	{
		AddTransition(m_OutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FlushBarriers();
	}

	RenderSkybox();

	// Switch resource states for lighting pass
	{
		// Switch render targets
		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			AddTransition(m_RenderTargets.at(rt), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
		AddTransition(GetCurrentDepthBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		AddTransition(m_OutputResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		FlushBarriers();
	}

	LightingPass();

	if (m_LightManager->IsUsingESM())
	{
		// ESM is used in volumetrics
		CreateESM();
	}

	if (m_UseVolumetrics)
	{
		RenderVolumetrics();
	}

	// Switch resource states back
	{
		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			AddTransition(m_RenderTargets.at(rt), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		FlushBarriers();
	}

	// Perform post-processing on uav
	{
		PIXBeginEvent(commandList, PIX_COLOR_INDEX(9), "Post Processing");

		Tonemapping();

		PIXEndEvent(commandList);
	}

	barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(m_OutputResource.GetResource()));
	FlushBarriers();

	// Update previous dimensions to current dimensions
	m_PrevGBufferWidth = m_GBufferWidth;
	m_PrevGBufferHeight = m_GBufferHeight;

	// Switch depth buffers
	m_CurrentDepthBuffer = 1 - m_CurrentDepthBuffer;
}


void DeferredRenderer::RenderShadowMaps() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(2), "Shadow Map");


	// Render directional light shadow map
	const auto& shadowMap = m_LightManager->GetSunShadowMap();
	const auto dsv = shadowMap.GetDSV();

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_DEPTH_WRITE);
	commandList->ResourceBarrier(1, &barrier);

	shadowMap.ApplyViewport(commandList);

	// Clear shadow map
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// Set depth stencil view and no render targets
	commandList->OMSetRenderTargets(0, nullptr, false, &dsv);

	m_DepthOnlyPipeline.Bind(commandList);
	commandList->SetGraphicsRootConstantBufferView(DepthPassRootSignature::LightConstantBuffer, m_LightManager->GetLightingConstantBuffer());

	DrawAllGeometry(commandList, DepthPassRootSignature::ObjectConstantBuffer);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	PIXEndEvent(commandList);
}

void DeferredRenderer::CreateESM() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(10), "ESM");
	const auto& shadowMap = m_LightManager->GetSunShadowMap();
	auto& esm = m_LightManager->GetSunESM();

	// First perform downsampling and transforming
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(esm.GetWriteResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);

		m_ESMDownsamplePipeline.Bind(commandList);

		const ExponentialShadowMapCB cb
		{
			.InDimensions = shadowMap.GetDimensions(),
			.OutDimensions = esm.GetDimensions(),
		};
		commandList->SetComputeRoot32BitConstants(ESMDownsampleTransformRootSignature::ParamsCB, SizeOfInUint32(cb), &cb, 0);
		commandList->SetComputeRootDescriptorTable(ESMDownsampleTransformRootSignature::InShadowMap, shadowMap.GetSRV());
		commandList->SetComputeRootDescriptorTable(ESMDownsampleTransformRootSignature::OutESM, esm.GetUAV());

		const UINT dispatchX = (cb.OutDimensions.x + 15) / 16;
		const UINT dispatchY = (cb.OutDimensions.y + 15) / 16;

		commandList->Dispatch(dispatchX, dispatchY, 1);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(esm.GetWriteResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
	}

	esm.FlipResources();
	// Then perform 11-texel wide separable box blur
	{
		const BoxFilterParamsCB cb
		{
			.ResourceDimensions = esm.GetDimensions(),
			.FilterSize = 3
		};

		for (const auto& pass : m_SeparableBoxFilterPipelines)
		{
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(esm.GetWriteResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commandList->ResourceBarrier(1, &barrier);

			pass.Bind(commandList);

			commandList->SetComputeRoot32BitConstants(SeparableBoxFilterRootSignature::ParamsCB, SizeOfInUint32(cb), &cb, 0);
			commandList->SetComputeRootDescriptorTable(SeparableBoxFilterRootSignature::InTexture, esm.GetSRV());
			commandList->SetComputeRootDescriptorTable(SeparableBoxFilterRootSignature::OutTexture, esm.GetUAV());

			const UINT dispatchX = (cb.ResourceDimensions.x + 15) / 16;
			const UINT dispatchY = (cb.ResourceDimensions.y + 15) / 16;

			commandList->Dispatch(dispatchX, dispatchY, 1);

			barrier = CD3DX12_RESOURCE_BARRIER::UAV(esm.GetWriteResource());
			commandList->ResourceBarrier(1, &barrier);

			barrier = CD3DX12_RESOURCE_BARRIER::Transition(esm.GetWriteResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			commandList->ResourceBarrier(1, &barrier);

			esm.FlipResources();
		}
	}
	PIXEndEvent(commandList);
}



void DeferredRenderer::ClearGBuffer() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(1), "Clear GBuffer");

	// Clear g buffer
	for (UINT rt = 0; rt < s_RTCount; rt++)
	{
		constexpr XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		commandList->ClearRenderTargetView(m_RTVs.GetCPUHandle(rt), &clearColor.x, 0, nullptr);
	}
	commandList->ClearDepthStencilView(m_DSVs.GetCPUHandle(m_CurrentDepthBuffer), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	PIXEndEvent(commandList);
}

void DeferredRenderer::GeometryPass() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(2), "Geometry Pass");

	// Assign render targets and depth buffer
	const auto firstRTV = m_RTVs.GetCPUHandle();
	const auto dsv = m_DSVs.GetCPUHandle(m_CurrentDepthBuffer);
	commandList->OMSetRenderTargets(s_RTCount, &firstRTV, true, &dsv);

	// Perform drawing into g-buffer
	m_GeometryPassPipeline.Bind(commandList);

	commandList->SetGraphicsRootConstantBufferView(GeometryPassRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetGraphicsRootShaderResourceView(GeometryPassRootSignature::MaterialBuffer, m_MaterialManager->GetMaterialBufferAddress());

	DrawAllGeometry(commandList, GeometryPassRootSignature::ObjectConstantBuffer);

	PIXEndEvent(commandList);
}

void DeferredRenderer::RenderSkybox() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	const auto ibl = m_LightManager->GetIBL();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(4), "Render Skybox");

	// Assign render targets and depth buffer
	const auto rtv = m_OutputRTV.GetCPUHandle();
	const auto dsv = m_DSVs.GetCPUHandle(m_CurrentDepthBuffer);
	commandList->OMSetRenderTargets(1, &rtv, true, &dsv);

	// Bind pipeline state
	m_SkyboxPipeline.Bind(commandList);

	commandList->SetGraphicsRootConstantBufferView(SkyboxPassRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	constexpr FullscreenQuadProperties quadProperties{ 1.0f };
	commandList->SetGraphicsRoot32BitConstants(SkyboxPassRootSignature::QuadProperties, SizeOfInUint32(quadProperties), &quadProperties, 0);

	commandList->SetGraphicsRootDescriptorTable(SkyboxPassRootSignature::EnvironmentMap, ibl->GetEnvironmentMapSRV());
	commandList->SetGraphicsRootDescriptorTable(SkyboxPassRootSignature::EnvironmentMapSampler, ibl->GetSamplerTable());

	// Triangle-strip for full-screen quad
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 0, nullptr);
	commandList->IASetIndexBuffer(nullptr);

	// Draw 4 vertices
	commandList->DrawInstanced(4, 1, 0, 0);

	PIXEndEvent(commandList);
}

void DeferredRenderer::LightingPass() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	const auto ibl = m_LightManager->GetIBL();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(3), "Lighting Pass");

	// Lighting pass
	m_LightingPipeline.Bind(commandList);

	// Set root arguments
	commandList->SetComputeRootConstantBufferView(LightingPassRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::GBuffer, m_SRVs.GetGPUHandle());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::DepthBuffer, GetCurrentDepthBufferSRV());
	commandList->SetComputeRootConstantBufferView(LightingPassRootSignature::LightingConstantBuffer, m_LightManager->GetLightingConstantBuffer());
	commandList->SetComputeRootShaderResourceView(LightingPassRootSignature::PointLightBuffer, m_LightManager->GetPointLightBuffer());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::EnvironmentMaps, ibl->GetSRVTable());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::EnvironmentSamplers, ibl->GetSamplerTable());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::SunShadowMap, m_LightManager->GetSunShadowMap().GetSRV());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::ShadowMapSampler, m_LightManager->GetShadowSampler(LightManager::ShadowSampler_Comparison));
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::OutputResource, m_OutputUAV.GetGPUHandle());

	// Dispatch lighting pass
	// Uses 8 threads per group (fast ceiling of integer division)
	const UINT threadGroupX = (m_GBufferWidth + 7) / 8;
	const UINT threadGroupY = (m_GBufferHeight + 7) / 8;

	commandList->Dispatch(threadGroupX, threadGroupY, 1);

	PIXEndEvent(commandList);
}


void DeferredRenderer::RenderVolumetrics() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(11), "Render Volumetrics");

	// Perform volume rendering
	{
		
		const VolumetricRendering::RenderVolumetricsParams params
		{
			.DepthBuffer = GetCurrentDepthBufferSRV(),
			.DepthBufferDimensions = { m_GBufferWidth, m_GBufferHeight },
			.PreviousDepthBuffer = GetPreviousDepthBufferSRV(),
			.PreviousDepthBufferDimensions = { m_PrevGBufferWidth, m_PrevGBufferHeight }
		};
		m_VolumeRenderer->RenderVolumetrics(params);
	}

	// Apply volumetric rendering
	{
		const VolumetricRendering::ApplyVolumetricsParams params
		{
			.OutputUAV = m_OutputUAV.GetGPUHandle(),
			.DepthBufferSRV = GetCurrentDepthBufferSRV(),
			.OutputResolution = { m_GBufferWidth, m_GBufferHeight }
		};
		m_VolumeRenderer->ApplyVolumetrics(params);
	}

	const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_OutputResource.GetResource());
	commandList->ResourceBarrier(1, &barrier);

	PIXEndEvent(commandList);
}


void DeferredRenderer::Tonemapping() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(8), "Tonemapping");

	// Lighting pass
	m_TonemappingPipeline.Bind(commandList);

	const UINT clientWidth = g_D3DGraphicsContext->GetClientWidth();
	const UINT clientHeight = g_D3DGraphicsContext->GetClientHeight();
	const TonemappingParametersConstantBuffer cb
	{
		.OutputDimensions = { clientWidth, clientHeight }
	};

	// Set root arguments
	commandList->SetComputeRoot32BitConstants(TonemappingRootSignature::TonemappingParametersConstantBuffer, SizeOfInUint32(cb), &cb, 0);
	commandList->SetComputeRootDescriptorTable(TonemappingRootSignature::OutputResource, m_OutputUAV.GetGPUHandle());

	// Uses 8 threads per group (fast ceiling of integer division)
	const UINT threadGroupX = (clientWidth + 7) / 8;
	const UINT threadGroupY = (clientHeight + 7) / 8;

	commandList->Dispatch(threadGroupX, threadGroupY, 1);

	PIXEndEvent(commandList);
}


void DeferredRenderer::DrawAllGeometry(ID3D12GraphicsCommandList* commandList, UINT objectCBParamIndex) const
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (const auto& geometryInstance : m_Scene->GetAllGeometryInstances())
	{
		// Set material root parameters
		commandList->SetGraphicsRootConstantBufferView(objectCBParamIndex, m_Scene->GetObjectCBAddress(geometryInstance.GetInstanceID()));

		// Bind geometry
		const TriangleGeometry* geometry = geometryInstance.GetGeometry();

		commandList->IASetIndexBuffer(&geometry->IndexBufferView);
		commandList->IASetVertexBuffers(0, 1, &geometry->VertexBufferView);

		commandList->DrawIndexedInstanced(geometry->IndexBuffer.GetElementCount(), 1, 0, 0, 0);
	}
}


void DeferredRenderer::DrawGui()
{
	ImGui::Text("Renderer");

	{
		ImGui::Checkbox("Use Volumetrics", &m_UseVolumetrics);
		GuiHelpers::DisableScope disable(!m_UseVolumetrics);
		m_VolumeRenderer->DrawGui();
	}
}
