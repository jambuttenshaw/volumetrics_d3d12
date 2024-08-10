#include "pch.h"
#include "VolumetricRendering.h"

#include "Light.h"
#include "Renderer/D3DGraphicsContext.h"


namespace DensityEstimationRootSignature
{
	enum Parameters
	{
		VBuffer = 0,
		Count
	};
}

namespace LightScatteringRootSignature
{
	enum Parameters
	{
		PassConstantBuffer,
		LightConstantBuffer,
		VBuffer,
		LightScatteringVolume,
		Count
	};
}

namespace VolumeIntegrationRootSignature
{
	enum Parameters
	{
		LightScatteringVolume = 0,
		Count
	};
}


VolumetricRendering::VolumetricRendering(LightManager& lightManager)
	: m_LightManager(&lightManager)
{
	ASSERT(
		m_VolumeResolution.x % 8 == 0 &&
		m_VolumeResolution.y % 8 == 0 &&
		m_VolumeResolution.z % 8 == 0,
		"Invalid volume resolution");

	m_DispatchGroups = {
		m_VolumeResolution.x / 8,
		m_VolumeResolution.y / 8,
		m_VolumeResolution.z / 8
	};

	CreateResources();
	CreatePipelines();
}

VolumetricRendering::~VolumetricRendering()
{
	m_Descriptors.Free();
}


void VolumetricRendering::CreateResources()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	const auto desc = CD3DX12_RESOURCE_DESC::Tex3D(
		m_Format,
		m_VolumeResolution.x,
		m_VolumeResolution.y,
		m_VolumeResolution.z,
		1,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	m_VBufferA.Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_VBufferB.Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	m_LightScatteringVolume.Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


	// Create descriptors
	m_Descriptors = g_D3DGraphicsContext->GetSRVHeap()->Allocate(DescriptorIndices::DescriptorCount);
	ASSERT(m_Descriptors.IsValid(), "Failed to alloc!");

	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = m_Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = 1;
		srvDesc.Texture3D.MostDetailedMip = 0;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

		auto CreateSRV = [&](const auto& resource, const auto& descriptorIndex)
			{
				device->CreateShaderResourceView(resource.GetResource(), &srvDesc, m_Descriptors.GetCPUHandle(descriptorIndex));
			};

		CreateSRV(m_VBufferA, SRV_VBufferA);
		CreateSRV(m_VBufferB, SRV_VBufferB);
		CreateSRV(m_LightScatteringVolume, SRV_LightScatteringVolume);
	}

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = m_Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = 0;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = -1;

		auto CreateUAV = [&](const auto& resource, const auto& descriptorIndex)
			{
				device->CreateUnorderedAccessView(resource.GetResource(), nullptr, &uavDesc, m_Descriptors.GetCPUHandle(descriptorIndex));
			};

		CreateUAV(m_VBufferA, UAV_VBufferA);
		CreateUAV(m_VBufferB, UAV_VBufferB);
		CreateUAV(m_LightScatteringVolume, UAV_LightScatteringVolume);
	}

}

void VolumetricRendering::CreatePipelines()
{
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[DensityEstimationRootSignature::Count];
		rootParams[DensityEstimationRootSignature::VBuffer].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = 0,
			.RootParameters = rootParams,
			.Shader = L"assets/shaders/compute/volumetrics/density_estimation_cs.hlsl",
			.EntryPoint = L"main"
		};

		m_DensityEstimationPipeline.Create(&psoDesc);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[LightScatteringRootSignature::Count];
		rootParams[LightScatteringRootSignature::PassConstantBuffer].InitAsConstantBufferView(0);
		rootParams[LightScatteringRootSignature::LightConstantBuffer].InitAsConstantBufferView(1);
		rootParams[LightScatteringRootSignature::VBuffer].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[LightScatteringRootSignature::LightScatteringVolume].InitAsDescriptorTable(1, &ranges[1]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = 0,
			.RootParameters = rootParams,
			.Shader = L"assets/shaders/compute/volumetrics/light_scattering_cs.hlsl",
			.EntryPoint = L"main"
		};

		m_LightScatteringPipeline.Create(&psoDesc);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[VolumeIntegrationRootSignature::Count];
		rootParams[VolumeIntegrationRootSignature::LightScatteringVolume].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = 0,
			.RootParameters = rootParams,
			.Shader = L"assets/shaders/compute/volumetrics/volume_integration_cs.hlsl",
			.EntryPoint = L"main"
		};

		m_VolumeIntegrationPipeline.Create(&psoDesc);
	}
}


void VolumetricRendering::RenderVolumetrics() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	auto FlushBarriers = [&]()
	{
		commandList->ResourceBarrier(barriers.size(), barriers.data());
		barriers.clear();
	};
	auto AddBarrier = [&](const auto& resource, auto before, auto after)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource.GetResource(), before, after));
		};

	AddBarrier(m_VBufferA, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	AddBarrier(m_VBufferB, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	FlushBarriers();

	DensityEstimation();

	AddBarrier(m_VBufferA, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	AddBarrier(m_VBufferB, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	AddBarrier(m_LightScatteringVolume, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	FlushBarriers();

	LightScattering();

	VolumeIntegration();

	AddBarrier(m_LightScatteringVolume, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	FlushBarriers();
}


void VolumetricRendering::DensityEstimation() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	m_DensityEstimationPipeline.Bind(commandList);

	commandList->SetComputeRootDescriptorTable(DensityEstimationRootSignature::VBuffer, m_Descriptors.GetGPUHandle(UAV_VBufferA));

	commandList->Dispatch(m_DispatchGroups.x, m_DispatchGroups.y, m_DispatchGroups.z);
}

void VolumetricRendering::LightScattering() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	m_LightScatteringPipeline.Bind(commandList);

	commandList->SetComputeRootConstantBufferView(LightScatteringRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootConstantBufferView(LightScatteringRootSignature::LightConstantBuffer, m_LightManager->GetLightingConstantBuffer());
	commandList->SetComputeRootDescriptorTable(LightScatteringRootSignature::VBuffer, m_Descriptors.GetGPUHandle(SRV_VBufferA));
	commandList->SetComputeRootDescriptorTable(LightScatteringRootSignature::LightScatteringVolume, m_Descriptors.GetGPUHandle(UAV_LightScatteringVolume));

	commandList->Dispatch(m_DispatchGroups.x, m_DispatchGroups.y, m_DispatchGroups.z);
}

void VolumetricRendering::VolumeIntegration() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	m_VolumeIntegrationPipeline.Bind(commandList);

	commandList->SetComputeRootDescriptorTable(VolumeIntegrationRootSignature::LightScatteringVolume, m_Descriptors.GetGPUHandle(UAV_LightScatteringVolume));

	commandList->Dispatch(m_DispatchGroups.x, m_DispatchGroups.y, m_DispatchGroups.z);
}
