#include "pch.h"
#include "VolumetricRendering.h"

#include <pix3.h>

#include "imgui.h"

#include "Light.h"
#include "Renderer/D3DGraphicsContext.h"


namespace DensityEstimationRootSignature
{
	enum Parameters
	{
		PassConstantBuffer = 0,
		VolumeConstantBuffer,
		GlobalFogConstantBuffer,
		VBuffer,
		Count
	};
}

namespace LightScatteringRootSignature
{
	enum Parameters
	{
		PassConstantBuffer = 0,
		LightConstantBuffer,
		VolumeConstantBuffer,
		VBuffer,
		SunShadowMap,
		ShadowSampler,
		LightScatteringVolume,
		Count
	};
}

namespace VolumeIntegrationRootSignature
{
	enum Parameters
	{
		PassConstantBuffer = 0,
		VolumeConstantBuffer,
		LightScatteringVolume,
		Count
	};
}

namespace ApplyVolumetricsRootSignature
{
	enum Parameters
	{
		PassConstantBuffer = 0,
		VolumeConstantBuffer,
		LightScatteringVolume,
		DepthBuffer,
		LightScatteringVolumeSampler,
		Output,
		Count
	};
}


VolumetricRendering::VolumetricRendering(const LightManager& lightManager)
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

	m_GlobalFogStagingBuffer.Albedo = XMFLOAT3(3.0f, 3.0f, 3.0f);
	m_GlobalFogStagingBuffer.Extinction = 0.5f;
}

VolumetricRendering::~VolumetricRendering()
{
	m_Descriptors.Free();
	m_LightVolumeSampler.Free();
}


void VolumetricRendering::CreateResources()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Allocate volume resources
	{
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
	}


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


	// Create constant buffer
	m_VolumeConstantBuffer.Allocate(device, 1, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Volume Constant Buffer");
	VolumetricsConstantBuffer cb
	{
		.VolumeResolution = m_VolumeResolution,
		.MaxVolumeDistance = m_MaxVolumeDistance
	};
	m_VolumeConstantBuffer.CopyElement(0, cb);

	m_GlobalFogConstantBuffer.Allocate(device, D3DGraphicsContext::GetBackBufferCount(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Global Fog Constant Buffer");


	// Create volume sampler
	m_LightVolumeSampler = g_D3DGraphicsContext->GetSamplerHeap()->Allocate(1);
	ASSERT(m_LightVolumeSampler.IsValid(), "Failed to alloc!");
	{
		D3D12_SAMPLER_DESC desc = {};
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 0;
		desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BorderColor[0] = 0.0f;
		desc.BorderColor[1] = 0.0f;
		desc.BorderColor[2] = 0.0f;
		desc.BorderColor[3] = 0.0f;
		desc.MinLOD = 0.0f;
		desc.MaxLOD = 0.0f;

		device->CreateSampler(&desc, m_LightVolumeSampler.GetCPUHandle());
	}
}

void VolumetricRendering::CreatePipelines()
{
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[DensityEstimationRootSignature::Count];
		rootParams[DensityEstimationRootSignature::PassConstantBuffer].InitAsConstantBufferView(0);
		rootParams[DensityEstimationRootSignature::VolumeConstantBuffer].InitAsConstantBufferView(1);
		rootParams[DensityEstimationRootSignature::GlobalFogConstantBuffer].InitAsConstantBufferView(2);
		rootParams[DensityEstimationRootSignature::VBuffer].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = ARRAYSIZE(rootParams),
			.RootParameters = rootParams,
			.Shader = L"assets/shaders/compute/volumetrics/density_estimation_cs.hlsl",
			.EntryPoint = L"main"
		};

		m_DensityEstimationPipeline.Create(&psoDesc);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[4];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[LightScatteringRootSignature::Count];
		rootParams[LightScatteringRootSignature::PassConstantBuffer].InitAsConstantBufferView(0);
		rootParams[LightScatteringRootSignature::LightConstantBuffer].InitAsConstantBufferView(1);
		rootParams[LightScatteringRootSignature::VolumeConstantBuffer].InitAsConstantBufferView(2);
		rootParams[LightScatteringRootSignature::VBuffer].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[LightScatteringRootSignature::SunShadowMap].InitAsDescriptorTable(1, &ranges[1]);
		rootParams[LightScatteringRootSignature::ShadowSampler].InitAsDescriptorTable(1, &ranges[2]);
		rootParams[LightScatteringRootSignature::LightScatteringVolume].InitAsDescriptorTable(1, &ranges[3]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = ARRAYSIZE(rootParams),
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
		rootParams[VolumeIntegrationRootSignature::PassConstantBuffer].InitAsConstantBufferView(0);
		rootParams[VolumeIntegrationRootSignature::VolumeConstantBuffer].InitAsConstantBufferView(1);
		rootParams[VolumeIntegrationRootSignature::LightScatteringVolume].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = ARRAYSIZE(rootParams),
			.RootParameters = rootParams,
			.Shader = L"assets/shaders/compute/volumetrics/volume_integration_cs.hlsl",
			.EntryPoint = L"main"
		};

		m_VolumeIntegrationPipeline.Create(&psoDesc);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[4];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[ApplyVolumetricsRootSignature::Count];
		rootParams[ApplyVolumetricsRootSignature::PassConstantBuffer].InitAsConstantBufferView(0);
		rootParams[ApplyVolumetricsRootSignature::VolumeConstantBuffer].InitAsConstantBufferView(1);
		rootParams[ApplyVolumetricsRootSignature::LightScatteringVolume].InitAsDescriptorTable(1, &ranges[0]);
		rootParams[ApplyVolumetricsRootSignature::DepthBuffer].InitAsDescriptorTable(1, &ranges[1]);
		rootParams[ApplyVolumetricsRootSignature::LightScatteringVolumeSampler].InitAsDescriptorTable(1, &ranges[2]);
		rootParams[ApplyVolumetricsRootSignature::Output].InitAsDescriptorTable(1, &ranges[3]);

		D3DComputePipelineDesc psoDesc = {
			.NumRootParameters = ARRAYSIZE(rootParams),
			.RootParameters = rootParams,
			.Shader = L"assets/shaders/compute/volumetrics/apply_volumetrics_cs.hlsl",
			.EntryPoint = L"main"
		};

		m_ApplyVolumetricsPipeline.Create(&psoDesc);
	}
}


void VolumetricRendering::RenderVolumetrics() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	// Copy staging
	m_GlobalFogConstantBuffer.CopyElement(g_D3DGraphicsContext->GetCurrentBackBuffer(), m_GlobalFogStagingBuffer);

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
	auto AddUAV = [&](const auto& resource)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(resource.GetResource()));
		};

	AddTransition(m_VBufferA, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	AddTransition(m_VBufferB, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	FlushBarriers();

	DensityEstimation();

	AddUAV(m_VBufferA);
	AddUAV(m_VBufferB);
	AddTransition(m_VBufferA, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	AddTransition(m_VBufferB, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	AddTransition(m_LightScatteringVolume, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	FlushBarriers();

	LightScattering();

	AddUAV(m_LightScatteringVolume);
	FlushBarriers();

	VolumeIntegration();

	AddUAV(m_LightScatteringVolume);
	AddTransition(m_LightScatteringVolume, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	FlushBarriers();
}

void VolumetricRendering::ApplyVolumetrics(const ApplyVolumetricsParams& params) const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_INDEX(19), "Apply Volumetrics");

	m_ApplyVolumetricsPipeline.Bind(commandList);

	commandList->SetComputeRootConstantBufferView(ApplyVolumetricsRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootConstantBufferView(ApplyVolumetricsRootSignature::VolumeConstantBuffer, m_VolumeConstantBuffer.GetAddressOfElement(0));
	commandList->SetComputeRootDescriptorTable(ApplyVolumetricsRootSignature::LightScatteringVolume, m_Descriptors.GetGPUHandle(SRV_LightScatteringVolume));
	commandList->SetComputeRootDescriptorTable(ApplyVolumetricsRootSignature::DepthBuffer, params.DepthBufferSRV);
	commandList->SetComputeRootDescriptorTable(ApplyVolumetricsRootSignature::LightScatteringVolumeSampler, m_LightVolumeSampler.GetGPUHandle());
	commandList->SetComputeRootDescriptorTable(ApplyVolumetricsRootSignature::Output, params.OutputUAV);

	const UINT groupsX = (params.OutputResolution.x + 7) / 8;
	const UINT groupsY = (params.OutputResolution.y + 7) / 8;

	commandList->Dispatch(groupsX, groupsY, 1);

	PIXEndEvent(commandList);
}


void VolumetricRendering::DensityEstimation() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_INDEX(21), "Density Estimation");

	m_DensityEstimationPipeline.Bind(commandList);

	commandList->SetComputeRootConstantBufferView(DensityEstimationRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootConstantBufferView(DensityEstimationRootSignature::VolumeConstantBuffer, m_VolumeConstantBuffer.GetAddressOfElement(0));
	commandList->SetComputeRootConstantBufferView(DensityEstimationRootSignature::GlobalFogConstantBuffer, m_GlobalFogConstantBuffer.GetAddressOfElement(g_D3DGraphicsContext->GetCurrentBackBuffer()));
	commandList->SetComputeRootDescriptorTable(DensityEstimationRootSignature::VBuffer, m_Descriptors.GetGPUHandle(UAV_VBufferA));

	commandList->Dispatch(m_DispatchGroups.x, m_DispatchGroups.y, m_DispatchGroups.z);

	PIXEndEvent(commandList);
}

void VolumetricRendering::LightScattering() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_INDEX(22), "Light Scattering");

	m_LightScatteringPipeline.Bind(commandList);

	commandList->SetComputeRootConstantBufferView(LightScatteringRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootConstantBufferView(LightScatteringRootSignature::LightConstantBuffer, m_LightManager->GetLightingConstantBuffer());
	commandList->SetComputeRootConstantBufferView(LightScatteringRootSignature::VolumeConstantBuffer, m_VolumeConstantBuffer.GetAddressOfElement(0));
	commandList->SetComputeRootDescriptorTable(LightScatteringRootSignature::VBuffer, m_Descriptors.GetGPUHandle(SRV_VBufferA));
	commandList->SetComputeRootDescriptorTable(LightScatteringRootSignature::SunShadowMap, m_LightManager->GetSunShadowMap().GetSRV());
	commandList->SetComputeRootDescriptorTable(LightScatteringRootSignature::ShadowSampler, m_LightManager->GetShadowSampler());
	commandList->SetComputeRootDescriptorTable(LightScatteringRootSignature::LightScatteringVolume, m_Descriptors.GetGPUHandle(UAV_LightScatteringVolume));

	commandList->Dispatch(m_DispatchGroups.x, m_DispatchGroups.y, m_DispatchGroups.z);

	PIXEndEvent(commandList);
}

void VolumetricRendering::VolumeIntegration() const
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_INDEX(23), "Volume Integration");

	m_VolumeIntegrationPipeline.Bind(commandList);

	commandList->SetComputeRootConstantBufferView(VolumeIntegrationRootSignature::PassConstantBuffer, g_D3DGraphicsContext->GetPassCBAddress());
	commandList->SetComputeRootConstantBufferView(VolumeIntegrationRootSignature::VolumeConstantBuffer, m_VolumeConstantBuffer.GetAddressOfElement(0));
	commandList->SetComputeRootDescriptorTable(VolumeIntegrationRootSignature::LightScatteringVolume, m_Descriptors.GetGPUHandle(UAV_LightScatteringVolume));

	// Dispatch a single group along the z axis
	// A single slice will 'march' through the volume to accumulate values
	commandList->Dispatch(m_DispatchGroups.x, m_DispatchGroups.y, 1);

	PIXEndEvent(commandList);
}


void VolumetricRendering::DrawGui()
{
	ImGui::Text("Global Fog");

	XMFLOAT3 albedo = m_GlobalFogStagingBuffer.Albedo;
	if (ImGui::DragFloat3("Albedo", &albedo.x, 0.01f))
	{
		albedo.x = max(0.0f, albedo.x);
		albedo.y = max(0.0f, albedo.y);
		albedo.z = max(0.0f, albedo.z);
		m_GlobalFogStagingBuffer.Albedo = albedo;
	}

	if (ImGui::DragFloat("Extinction", &m_GlobalFogStagingBuffer.Extinction, 0.01f))
	{
		m_GlobalFogStagingBuffer.Extinction = max(0.0f, m_GlobalFogStagingBuffer.Extinction);
	}
}
