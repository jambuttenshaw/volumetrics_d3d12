#include "pch.h"
#include "DeferredRenderer.h"

#include "D3DGraphicsContext.h"

#include "Application/Scene.h"

#include "Lighting/Light.h"
#include "Lighting/Material.h"

#include "pix3.h"


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
		LightingConstantBuffer,	// Constant buffer with universal lighting parameters
		PointLightBuffer,		// A buffer of all point lights in the scene
		EnvironmentMaps,		// Environment maps are sequential
		EnvironmentSamplers,	// Environment samplers are sequential
		SunShadowMap,			// Shadow map for the sun
		OutputResource,
		Count
	};
}


DeferredRenderer::DeferredRenderer()
{
	CreatePipelines();
	CreateResolutionDependentResources();
}

DeferredRenderer::~DeferredRenderer()
{
	// Free descriptors
	m_RTVs.Free();
	m_DSV.Free();
	m_SRVs.Free();

	m_OutputUAV.Free();
	m_OutputRTV.Free();
}

void DeferredRenderer::SetScene(const Scene& scene, const LightManager& lightManager, const MaterialManager& materialManager)
{
	m_Scene = &scene;
	m_LightManager = &lightManager;
	m_MaterialManager = &materialManager;

	m_VolumeRenderer = std::make_unique<VolumetricRendering>(*m_LightManager);
}

void DeferredRenderer::OnBackBufferResize()
{
	m_RTVs.Free();
	m_DSV.Free();
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

		psoDesc.RasterizerDesc.DepthBias = 10000;
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

		CD3DX12_DESCRIPTOR_RANGE1 ranges[6];

		// All descriptors in the g-buffer are contiguous
		// There will be one for each render target plus the depth buffer
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, s_RTCount + 1, 0, 0);

		// Environment lighting descriptors
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1);

		// Environment samplers
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 3, 0, 0);

		// Shadow map
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 2);

		// Output UAV
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);


		// Set up root parameters
		CD3DX12_ROOT_PARAMETER1 rootParameters[LightingPassRootSignature::Count];
		rootParameters[LightingPassRootSignature::PassConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
		rootParameters[LightingPassRootSignature::GBuffer].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[LightingPassRootSignature::LightingConstantBuffer].InitAsConstantBufferView(1, 0);
		rootParameters[LightingPassRootSignature::PointLightBuffer].InitAsShaderResourceView(4, 0);
		rootParameters[LightingPassRootSignature::EnvironmentMaps].InitAsDescriptorTable(1, &ranges[1]);
		rootParameters[LightingPassRootSignature::EnvironmentSamplers].InitAsDescriptorTable(1, &ranges[2]);
		rootParameters[LightingPassRootSignature::SunShadowMap].InitAsDescriptorTable(1, &ranges[3]);
		rootParameters[LightingPassRootSignature::OutputResource].InitAsDescriptorTable(1, &ranges[4]);

		psoDesc.NumRootParameters = LightingPassRootSignature::Count;
		psoDesc.RootParameters = rootParameters;

		psoDesc.Shader = L"assets/shaders/gbuffer/lighting_pass_cs.hlsl";
		psoDesc.EntryPoint = L"main";

		m_LightingPipeline.Create(&psoDesc);
	}
}

void DeferredRenderer::CreateResolutionDependentResources()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Allocate render targets
	D3D12_RESOURCE_DESC desc;
	D3D12_CLEAR_VALUE clearValue;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = g_D3DGraphicsContext->GetClientWidth();
	desc.Height = g_D3DGraphicsContext->GetClientHeight();
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
		m_RenderTargets[rt].Allocate(&desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue);
	}

	// Allocate depth buffer
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Format = m_DSVFormat;

	clearValue.Format = m_DSVFormat;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	m_DepthBuffer.Allocate(&desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue);

	// Allocate output resource
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Format = m_OutputFormat;
	m_OutputResource.Allocate(&desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

	m_DSV = g_D3DGraphicsContext->GetDSVHeap()->Allocate(1);
	ASSERT(m_DSV.IsValid(), "Failed to alloc!");

	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_DSVFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(m_DepthBuffer.GetResource(), &dsvDesc, m_DSV.GetCPUHandle());
	}

	// Create SRVs
	m_SRVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(s_RTCount + 1);
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
		device->CreateShaderResourceView(m_DepthBuffer.GetResource(), &srvDesc, m_SRVs.GetCPUHandle(GB_SRV_Depth));
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


void DeferredRenderer::Render() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	auto FlushBarriers = [&]()
	{
		commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		barriers.clear();
	};

	// Render shadow maps first
	RenderShadowMaps();

	g_D3DGraphicsContext->RestoreDefaultViewport();

	// Then render geometry pass
	ClearGBuffer();

	GeometryPass();

	// After copying to the back-buffer, the output resource will be returned to unordered access state
	// It needs to be transitioned to a render target
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_OutputResource.GetResource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		FlushBarriers();
	}

	RenderSkybox();

	// Switch resource states for lighting pass
	{
		// Switch render targets
		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets.at(rt).GetResource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		}
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			m_DepthBuffer.GetResource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_OutputResource.GetResource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		FlushBarriers();
	}

	LightingPass();

	// Perform volume rendering
	m_VolumeRenderer->RenderVolumetrics();
	const VolumetricRendering::ApplyVolumetricsParams params
	{
		.OutputUAV = m_OutputUAV.GetGPUHandle(),
		.DepthBufferSRV = m_SRVs.GetGPUHandle(GB_SRV_Depth),
		.OutputResolution = { g_D3DGraphicsContext->GetClientWidth(), g_D3DGraphicsContext->GetClientHeight() }
	};
	m_VolumeRenderer->ApplyVolumetrics(params);


	// Switch resource states back
	{
		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets.at(rt).GetResource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET));
		}
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			m_DepthBuffer.GetResource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE));
		FlushBarriers();
	}
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
	commandList->ClearDepthStencilView(m_DSV.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	PIXEndEvent(commandList);
}

void DeferredRenderer::GeometryPass() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(2), "Geometry Pass");

	// Assign render targets and depth buffer
	const auto firstRTV = m_RTVs.GetCPUHandle();
	const auto dsv = m_DSV.GetCPUHandle();
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

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(4), "Render Skybox");

	// Assign render targets and depth buffer
	const auto rtv = m_OutputRTV.GetCPUHandle();
	const auto dsv = m_DSV.GetCPUHandle();
	commandList->OMSetRenderTargets(1, &rtv, true, &dsv);

	// Bind pipeline state
	m_SkyboxPipeline.Bind(commandList);

	commandList->SetGraphicsRootConstantBufferView(SkyboxPassRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	constexpr FullscreenQuadProperties quadProperties{ 1.0f };
	commandList->SetGraphicsRoot32BitConstants(SkyboxPassRootSignature::QuadProperties, SizeOfInUint32(quadProperties), &quadProperties, 0);

	commandList->SetGraphicsRootDescriptorTable(SkyboxPassRootSignature::EnvironmentMap, m_LightManager->GetEnvironmentMapSRV());
	commandList->SetGraphicsRootDescriptorTable(SkyboxPassRootSignature::EnvironmentMapSampler, m_LightManager->GetSamplerTable());

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

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(3), "Lighting Pass");

	// Lighting pass
	m_LightingPipeline.Bind(commandList);

	// Set root arguments
	commandList->SetComputeRootConstantBufferView(LightingPassRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::GBuffer, m_SRVs.GetGPUHandle());
	commandList->SetComputeRootConstantBufferView(LightingPassRootSignature::LightingConstantBuffer, m_LightManager->GetLightingConstantBuffer());
	commandList->SetComputeRootShaderResourceView(LightingPassRootSignature::PointLightBuffer, m_LightManager->GetPointLightBuffer());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::EnvironmentMaps, m_LightManager->GetSRVTable());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::EnvironmentSamplers, m_LightManager->GetSamplerTable());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::SunShadowMap, m_LightManager->GetSunShadowMap().GetSRV());
	commandList->SetComputeRootDescriptorTable(LightingPassRootSignature::OutputResource, m_OutputUAV.GetGPUHandle());

	// Dispatch lighting pass
	const UINT clientWidth = g_D3DGraphicsContext->GetClientWidth();
	const UINT clientHeight = g_D3DGraphicsContext->GetClientHeight();
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
