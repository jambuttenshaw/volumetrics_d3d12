#pragma once
#include "Core.h"
#include "HlslCompat/StructureHlslCompat.h"
#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/Texture.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Memory/MemoryAllocator.h"

using namespace DirectX;

class LightManager;


class VolumetricRendering
{
	enum DescriptorIndices
	{
		SRV_VBufferA = 0,
		SRV_VBufferB,
		SRV_LightScatteringVolume,
		UAV_VBufferA,
		UAV_VBufferB,
		UAV_LightScatteringVolume,
		DescriptorCount
	};

public:
	struct ApplyVolumetricsParams
	{
		D3D12_GPU_DESCRIPTOR_HANDLE OutputUAV;
		D3D12_GPU_DESCRIPTOR_HANDLE DepthBufferSRV;

		XMUINT2 OutputResolution;
	};

public:
	VolumetricRendering(LightManager& lightManager);
	~VolumetricRendering();

	DISALLOW_COPY(VolumetricRendering);
	DEFAULT_MOVE(VolumetricRendering);

	void RenderVolumetrics() const;
	void ApplyVolumetrics(const ApplyVolumetricsParams& params) const;


	void DrawGui();

private:
	void CreateResources();
	void CreatePipelines();

	// Volumetric Rendering sub-stages
	void DensityEstimation() const;
	void LightScattering() const;
	void VolumeIntegration() const;

private:
	XMUINT3 m_VolumeResolution = { 512, 288, 256 };
	XMUINT3 m_DispatchGroups;

	float m_MaxVolumeDistance = 100.0f;

	DXGI_FORMAT m_Format = DXGI_FORMAT_R16G16B16A16_FLOAT;


	// Volume resources

	/*
	 * Format: RGBA16F
	 * RGB - Scattering
	 * A   - Extinction
	*/
	Texture m_VBufferA;
	/*
	 * Format: RGBA16F
	 * RGB - Emissive
	 * A   - Phase (g)
	*/
	Texture m_VBufferB;

	/*
	 * Format: RGBA16F
	 * RGB - Scattered Light to Camera
	 * A   - Extinction
	*/
	Texture m_LightScatteringVolume;

	UploadBuffer<VolumetricsConstantBuffer> m_VolumeConstantBuffer;
	UploadBuffer<GlobalFogConstantBuffer> m_GlobalFogConstantBuffer;

	// Pipelines
	D3DComputePipeline m_DensityEstimationPipeline;
	D3DComputePipeline m_LightScatteringPipeline;
	D3DComputePipeline m_VolumeIntegrationPipeline;
	D3DComputePipeline m_ApplyVolumetricsPipeline;

	// SRV and UAV for each volume
	DescriptorAllocation m_Descriptors;
	DescriptorAllocation m_LightVolumeSampler;


	LightManager* m_LightManager;

	GlobalFogConstantBuffer m_GlobalFogStagingBuffer;
};
