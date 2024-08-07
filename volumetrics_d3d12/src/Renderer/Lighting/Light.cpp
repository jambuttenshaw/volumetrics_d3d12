#include "pch.h"
#include "Light.h"


#include "Renderer/D3DGraphicsContext.h"
#include "HlslCompat/LightingHlslCompat.h"
#include "Framework/Math.h"

#include <pix3.h>
#include "imgui.h"


// Pipeline root signatures
namespace IrradianceMapPipelineSignature
{
	enum Value
	{
		ParamsBufferSlot = 0,
		EnvironmentMapSlot,
		EnvironmentSamplerSlot,
		IrradianceMapSlot,
		Count
	};
}

namespace BRDFIntegrationPipelineSignature
{
	enum Value
	{
		BRDFIntegrationMapSlot = 0,
		Count
	};
}

namespace PEMPipelineSignature
{
	enum Value
	{
		ParamsBufferSlot = 0,
		EnvironmentMapSlot,
		EnvironmentSamplerSlot,
		PrefilteredEnvironmentMapSlot,
		Count
	};
}


XMFLOAT3 g_CubemapFaceNormals[6] = {
		{ 1.0f, 0.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, -1.0f }
};
XMFLOAT3 g_CubemapFaceTangents[6] = {
	{ 0.0f, 0.0f, -1.0f },
	{ 0.0f, 0.0f, 1.0f },
	{ 1.0f, 0.0f, 0.0f },
	{ 1.0f, 0.0f, 0.0f },
	{ 1.0f, 0.0f, 0.0f },
	{ -1.0f, 0.0f, 0.0f }
};
XMFLOAT3 g_CubemapFaceBitangents[6] = {
	{ 0.0f, -1.0f, 0.0f },
	{ 0.0f, -1.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, -1.0f },
	{ 0.0f, -1.0f, 0.0f },
	{ 0.0f, -1.0f, 0.0f }
};


void CartesianDirectionToSpherical(const XMFLOAT3& direction, float& phi, float& theta)
{
	theta = acosf(direction.y);
	phi = Math::Sign(direction.z) * acosf(direction.x / sqrtf(direction.x * direction.x + direction.z * direction.z));
}


LightManager::LightManager()
{
	// Populate default light properties
	m_LightingCBStaging.DirectionalLight.Direction = { 0.0f, -0.9f, 0.1f };
	m_LightingCBStaging.DirectionalLight.Intensity = 2.0f;
	m_LightingCBStaging.DirectionalLight.Color = { 1.0f, 1.0f, 1.0f };

	m_LightingCBStaging.PointLightCount = s_MaxLights;

	for (auto& light : m_PointLightsStaging)
	{
		light.Position = { 0.0f, 1.5f, 0.0f };
		light.Color = { 1.0f, 1.0f, 1.0f };
		light.Intensity = 0.0f;
		light.Range = 5.0f;
	}

	// Set up shadow camera and shadow map
	m_ShadowCameraProjectionMatrix = XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, 100.0f);

	m_SunShadowMap.CreateShadowMap(1024, 1024);

	// Create API resources
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_CommandAllocator)));
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));

	THROW_IF_FAIL(m_CommandList->Close());

	m_GlobalLightingSRVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(SRVCount);
	ASSERT(m_GlobalLightingSRVs.IsValid(), "Descriptor allocation failed.");
	m_SamplerDescriptors = g_D3DGraphicsContext->GetSamplerHeap()->Allocate(SamplerCount);
	ASSERT(m_SamplerDescriptors.IsValid(), "Descriptor allocation failed.");

	m_IrradianceMapFaceUAVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(6);
	ASSERT(m_IrradianceMapFaceUAVs.IsValid(), "Descriptor allocation failed.");
	m_BRDFIntegrationMapUAV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
	ASSERT(m_BRDFIntegrationMapUAV.IsValid(), "Descriptor allocation failed.");
	m_PEMFaceUAVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(6 * m_PEMRoughnessBins);
	ASSERT(m_PEMFaceUAVs.IsValid(), "Descriptor allocation failed.");

	CreatePipelines();
	CreateResources();

	// Create light buffers
	m_LightingResources.resize(D3DGraphicsContext::GetBackBufferCount());
	for (auto& resources : m_LightingResources)
	{
		resources.LightingCB.Allocate(g_D3DGraphicsContext->GetDevice(), 1, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Lighting Constant Buffer");
		resources.PointLights.Allocate(g_D3DGraphicsContext->GetDevice(), s_MaxLights, 0, L"Point Light Buffer");
	}

}

LightManager::~LightManager()
{
	m_GlobalLightingSRVs.Free();
	m_SamplerDescriptors.Free();
	m_IrradianceMapFaceUAVs.Free();
	m_BRDFIntegrationMapUAV.Free();
	m_PEMFaceUAVs.Free();
}

void LightManager::UpdateLightingCB(const XMFLOAT3& eyePos)
{
	// Create a view matrix for the directional light
	XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&m_LightingCBStaging.DirectionalLight.Direction));
	XMVECTOR up = { 0.0f, 1.0f, 0.0f };

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
	up = XMVector3Cross(forward, right);

	// subtract a distance from the eye position to get a position for the camera
	constexpr float d = 10.0f;
	//XMVECTOR position = XMLoadFloat3(&eyePos) - d * forward;
	XMVECTOR position = - d * forward;

	XMMATRIX view = XMMatrixLookAtLH(position, position + forward, up);

	// Now update the CB with the new camera matrix
	const XMMATRIX viewProj = XMMatrixMultiply(view, m_ShadowCameraProjectionMatrix);
	m_LightingCBStaging.DirectionalLight.ViewProjection = XMMatrixTranspose(viewProj);
}

void LightManager::CopyStagingBuffers() const
{
	const auto& resources = m_LightingResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer());

	resources.LightingCB.CopyElement(0, m_LightingCBStaging);
	resources.PointLights.CopyElements(0, static_cast<UINT>(m_PointLightsStaging.size()), m_PointLightsStaging.data());
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetLightingConstantBuffer() const
{
	return m_LightingResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).LightingCB.GetAddressOfElement(0);
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetPointLightBuffer() const
{
	return m_LightingResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).PointLights.GetAddressOfElement(0);
}


void LightManager::CreatePipelines()
{
	{
		using namespace IrradianceMapPipelineSignature;

		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[ParamsBufferSlot].InitAsConstants(SizeOfInUint32(GlobalLightingParamsBuffer), 0);
		rootParams[EnvironmentMapSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[EnvironmentSamplerSlot].InitAsDescriptorTable(1, &ranges[1]);
		rootParams[IrradianceMapSlot].InitAsDescriptorTable(1, &ranges[2]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/environment/irradiance.hlsl";
		desc.EntryPoint = L"main";

		m_Pipelines[GlobalLightingPipeline::IrradianceMap] = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace BRDFIntegrationPipelineSignature;

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BRDFIntegrationMapSlot].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/environment/brdf_integration.hlsl";
		desc.EntryPoint = L"main";

		m_Pipelines[GlobalLightingPipeline::BRDFIntegration] = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace PEMPipelineSignature;

		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[ParamsBufferSlot].InitAsConstants(SizeOfInUint32(GlobalLightingParamsBuffer), 0);
		rootParams[EnvironmentMapSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[EnvironmentSamplerSlot].InitAsDescriptorTable(1, &ranges[1]);
		rootParams[PrefilteredEnvironmentMapSlot].InitAsDescriptorTable(1, &ranges[2]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/environment/prefiltered_environment.hlsl";
		desc.EntryPoint = L"main";

		m_Pipelines[GlobalLightingPipeline::PreFilteredEnvironmentMap] = std::make_unique<D3DComputePipeline>(&desc);
	}
}

void LightManager::CreateResources()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	{
		// Create irradiance map
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R8G8B8A8_UNORM, 
			m_IrradianceMapResolution, 
			m_IrradianceMapResolution, 
			6,
			1);
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		m_IrradianceMap = std::make_unique<Texture>(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// Create SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Format = desc.Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(m_IrradianceMap->GetResource(), &srvDesc, m_GlobalLightingSRVs.GetCPUHandle(IrradianceMapSRV));

		// Create UAVs
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.ArraySize = 1;
		uavDesc.Texture2DArray.PlaneSlice = 0;

		for (UINT face = 0; face < 6; face++)
		{
			uavDesc.Texture2DArray.FirstArraySlice = face;
			device->CreateUnorderedAccessView(m_IrradianceMap->GetResource(), nullptr, &uavDesc, m_IrradianceMapFaceUAVs.GetCPUHandle(face));
		}
	}

	{
		// Create the BRDF integration map
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R32G32_FLOAT,
			m_BRDFIntegrationMapResolution,
			m_BRDFIntegrationMapResolution,
			1,
			1);
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		m_BRDFIntegrationMap = std::make_unique<Texture>(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// Create SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = desc.Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(m_BRDFIntegrationMap->GetResource(), &srvDesc, m_GlobalLightingSRVs.GetCPUHandle(BRDFIntegrationMapSRV));

		// Create UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;

		device->CreateUnorderedAccessView(m_BRDFIntegrationMap->GetResource(), nullptr, &uavDesc, m_BRDFIntegrationMapUAV.GetCPUHandle());
	}

	{
		// Create the pre-filtered environment map
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R8G8B8A8_UNORM,
			m_PEMResolution,
			m_PEMResolution,
			6,
			m_PEMRoughnessBins);
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		m_PreFilteredEnvironmentMap = std::make_unique<Texture>(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// Create SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Format = desc.Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = m_PEMRoughnessBins;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(m_PreFilteredEnvironmentMap->GetResource(), &srvDesc, m_GlobalLightingSRVs.GetCPUHandle(PreFilteredEnvironmentMapSRV));

		// Create UAVs
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize = 1;
		uavDesc.Texture2DArray.PlaneSlice = 0;

		for (UINT mip = 0; mip < m_PEMRoughnessBins; mip++)
		{
			uavDesc.Texture2DArray.MipSlice = mip;
			for (UINT face = 0; face < 6; face++)
			{
				uavDesc.Texture2DArray.FirstArraySlice = face;

				const UINT index = 6 * mip + face;
				device->CreateUnorderedAccessView(m_PreFilteredEnvironmentMap->GetResource(), nullptr, &uavDesc, m_PEMFaceUAVs.GetCPUHandle(index));
			}
		}
	}

	{
		// Create samplers
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor[0] = 0.0f;
		samplerDesc.BorderColor[1] = 0.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 0.0f;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

		device->CreateSampler(&samplerDesc, m_SamplerDescriptors.GetCPUHandle(EnvironmentMapSampler));

		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		device->CreateSampler(&samplerDesc, m_SamplerDescriptors.GetCPUHandle(BRDFIntegrationMapSampler));
	}
}


void LightManager::ProcessEnvironmentMap(std::unique_ptr<Texture>&& map)
{
	// Set up environment map
	m_EnvironmentMap = std::move(map);

	// Create cubemap descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Format = m_EnvironmentMap->GetFormat();
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	g_D3DGraphicsContext->GetDevice()->CreateShaderResourceView(m_EnvironmentMap->GetResource(), &srvDesc, m_GlobalLightingSRVs.GetCPUHandle(EnvironmentMapSRV));

	GlobalLightingParamsBuffer params;
	auto SetParamsForFace = [&params](UINT face)
		{
			params.FaceNormal = g_CubemapFaceNormals[face];
			params.FaceTangent = g_CubemapFaceTangents[face];
			params.FaceBitangent = g_CubemapFaceBitangents[face];
		};

	// Begin processing
	// First perform synchronization

	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();
	// First, wait for rendering to finish since we are about to change resources that are potentially used in rendering
	computeQueue->InsertWaitForQueue(directQueue);
	THROW_IF_FAIL(m_CommandAllocator->Reset());
	THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

	
	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(56), L"Environment processing");


	ID3D12DescriptorHeap* ppHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap(), g_D3DGraphicsContext->GetSamplerHeap()->GetHeap() };
	m_CommandList->SetDescriptorHeaps(ARRAYSIZE(ppHeaps), ppHeaps);

	{
		// Transition resources to unordered access view
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(m_IrradianceMap->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(m_BRDFIntegrationMap->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(m_PreFilteredEnvironmentMap->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		{
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(57), L"Irradiance Map");

			// Irradiance map
			m_Pipelines[GlobalLightingPipeline::IrradianceMap]->Bind(m_CommandList.Get());

			const UINT threadGroupX = (m_IrradianceMapResolution + IRRADIANCE_THREADS - 1) / IRRADIANCE_THREADS;
			const UINT threadGroupY = (m_IrradianceMapResolution + IRRADIANCE_THREADS - 1) / IRRADIANCE_THREADS;

			m_CommandList->SetComputeRootDescriptorTable(IrradianceMapPipelineSignature::EnvironmentMapSlot, m_GlobalLightingSRVs.GetGPUHandle(EnvironmentMapSRV));
			m_CommandList->SetComputeRootDescriptorTable(IrradianceMapPipelineSignature::EnvironmentSamplerSlot, m_SamplerDescriptors.GetGPUHandle(EnvironmentMapSampler));
			for (UINT face = 0; face < 6; face++)
			{
				SetParamsForFace(face);

				m_CommandList->SetComputeRoot32BitConstants(IrradianceMapPipelineSignature::ParamsBufferSlot, SizeOfInUint32(GlobalLightingParamsBuffer), &params, 0);
				m_CommandList->SetComputeRootDescriptorTable(IrradianceMapPipelineSignature::IrradianceMapSlot, m_IrradianceMapFaceUAVs.GetGPUHandle(face));

				m_CommandList->Dispatch(threadGroupX, threadGroupY, 1);
			}

			PIXEndEvent(m_CommandList.Get());
		}

		{
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(58), L"BRDF Integration");

			// BRDF integration map
			m_Pipelines[GlobalLightingPipeline::BRDFIntegration]->Bind(m_CommandList.Get());

			const UINT threadGroupX = (m_BRDFIntegrationMapResolution + BRDF_INTEGRATION_THREADS - 1) / BRDF_INTEGRATION_THREADS;
			const UINT threadGroupY = (m_BRDFIntegrationMapResolution + BRDF_INTEGRATION_THREADS - 1) / BRDF_INTEGRATION_THREADS;

			m_CommandList->SetComputeRootDescriptorTable(BRDFIntegrationPipelineSignature::BRDFIntegrationMapSlot, m_BRDFIntegrationMapUAV.GetGPUHandle());
			m_CommandList->Dispatch(threadGroupX, threadGroupY, 1);

			PIXEndEvent(m_CommandList.Get());
		}


		{
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(59), L"Pre-Filtered Environment Map");

			// Prefiltered environment map
			m_Pipelines[GlobalLightingPipeline::PreFilteredEnvironmentMap]->Bind(m_CommandList.Get());

			m_CommandList->SetComputeRootDescriptorTable(PEMPipelineSignature::EnvironmentMapSlot, m_GlobalLightingSRVs.GetGPUHandle(EnvironmentMapSRV));
			m_CommandList->SetComputeRootDescriptorTable(PEMPipelineSignature::EnvironmentSamplerSlot, m_SamplerDescriptors.GetGPUHandle(EnvironmentMapSampler));
			for (UINT roughnessBin = 0; roughnessBin < m_PEMRoughnessBins; roughnessBin++)
			{
				const UINT mipSize = static_cast<UINT>(m_PEMResolution * std::powf(0.5f, static_cast<float>(roughnessBin)));

				const UINT threadGroupX = (mipSize + PREFILTERED_ENVIRONMENT_THREADS - 1) / PREFILTERED_ENVIRONMENT_THREADS;
				const UINT threadGroupY = (mipSize + PREFILTERED_ENVIRONMENT_THREADS - 1) / PREFILTERED_ENVIRONMENT_THREADS;

				params.Roughness = static_cast<float>(roughnessBin) / static_cast<float>(m_PEMRoughnessBins - 1);
				for (UINT face = 0; face < 6; face++)
				{
					SetParamsForFace(face);
					const UINT index = 6 * roughnessBin + face;

					m_CommandList->SetComputeRoot32BitConstants(PEMPipelineSignature::ParamsBufferSlot, SizeOfInUint32(GlobalLightingParamsBuffer), &params, 0);
					m_CommandList->SetComputeRootDescriptorTable(PEMPipelineSignature::PrefilteredEnvironmentMapSlot, m_PEMFaceUAVs.GetGPUHandle(index));

					m_CommandList->Dispatch(threadGroupX, threadGroupY, 1);
				}
			}

			PIXEndEvent(m_CommandList.Get());

		}

		// Transition resources back to non-pixel shader resource
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(m_IrradianceMap->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(m_BRDFIntegrationMap->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(m_PreFilteredEnvironmentMap->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(m_EnvironmentMap->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}
	}

	PIXEndEvent(m_CommandList.Get());

	// Execute work
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_PreviousWorkFence = computeQueue->ExecuteCommandLists(ARRAYSIZE(ppCommandLists), ppCommandLists);

	// Now make sure that rendering waits for compute to finish, so that it doesn't try to render resources as we are modifying them
	directQueue->InsertWaitForQueue(computeQueue);
	// Wait until completion
	computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
}


void LightManager::DrawGui()
{

	// Directional Light
	{
		ImGui::Text("Directional Light");

		auto& directionalLight = m_LightingCBStaging.DirectionalLight;

		// Direction should be edited in spherical coordinates
		bool newDir = false;
		float phi, theta;
		CartesianDirectionToSpherical(directionalLight.Direction, phi, theta);

		newDir |= ImGui::SliderAngle("Theta", &theta, 1.0f, 179.0f);
		newDir |= ImGui::SliderAngle("Phi", &phi, -179.0f, 180.0f);
		if (newDir)
		{
			const float sinTheta = sinf(theta);
			const float cosTheta = cosf(theta);
			const float sinPhi = sinf(phi);
			const float cosPhi = cosf(phi);

			directionalLight.Direction.x = sinTheta * cosPhi;
			directionalLight.Direction.y = cosTheta;
			directionalLight.Direction.z = sinTheta * sinPhi;
		}

		ImGui::ColorEdit3("Sun Color", &directionalLight.Color.x);
		if (ImGui::DragFloat("Sun Intensity", &directionalLight.Intensity, 0.01f))
		{
			directionalLight.Intensity = max(0, directionalLight.Intensity);
		}
	}

	for (size_t i = 0; i < m_PointLightsStaging.size(); i++)
	{
		auto& light = m_PointLightsStaging.at(i);

		ImGui::Text("Light %d", i);

		ImGui::DragFloat3("Position", &light.Position.x, 0.01f);
		ImGui::ColorEdit3("Color", &light.Color.x);
		if (ImGui::DragFloat("Intensity", &light.Intensity, 0.01f))
		{
			light.Intensity = max(0, light.Intensity);
		}
	}

	if (ImGui::Begin("Shadow Map"))
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(m_SunShadowMap.GetSRV().ptr), { 400.0f, 400.0f });

		ImGui::End();
	}
}
