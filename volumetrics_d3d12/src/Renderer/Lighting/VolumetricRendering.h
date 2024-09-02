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
		SRV_LightScatteringVolume0,
		SRV_LightScatteringVolume1,
		SRV_IntegratedVolume,
		UAV_VBufferA,
		UAV_VBufferB,
		UAV_LightScatteringVolume0,
		UAV_LightScatteringVolume1,
		UAV_IntegratedVolume,
		DescriptorCount
	};

public:

	struct RenderVolumetricsParams
	{
		D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer;
		XMUINT2 DepthBufferDimensions;

		D3D12_GPU_DESCRIPTOR_HANDLE PreviousDepthBuffer;
		XMUINT2 PreviousDepthBufferDimensions;
	};

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

	void RenderVolumetrics(const RenderVolumetricsParams& params);
	void ApplyVolumetrics(const ApplyVolumetricsParams& params) const;


	void DrawGui();

private:
	void CreateResources();
	void CreatePipelines();

	// Volumetric Rendering sub-stages
	void DensityEstimation() const;
	void LightScattering(const RenderVolumetricsParams& params) const;
	void VolumeIntegration() const;

	// LSV = LightScatteringVolume
	inline const Texture& GetCurrentLSV() const { return m_LightScatteringVolumes.at(m_CurrentLightScatteringVolume); }
	inline const Texture& GetPreviousLSV() const { return m_LightScatteringVolumes.at(1 - m_CurrentLightScatteringVolume); }

	inline Texture& GetCurrentLSV() { return m_LightScatteringVolumes.at(m_CurrentLightScatteringVolume); }
	inline Texture& GetPreviousLSV() { return m_LightScatteringVolumes.at(1 - m_CurrentLightScatteringVolume); }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetCurrentLSV_SRV() const { return m_Descriptors.GetGPUHandle(SRV_LightScatteringVolume0 + m_CurrentLightScatteringVolume); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetCurrentLSV_UAV() const { return m_Descriptors.GetGPUHandle(UAV_LightScatteringVolume0 + m_CurrentLightScatteringVolume); }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPreviousLSV_SRV() const { return m_Descriptors.GetGPUHandle(SRV_LightScatteringVolume0 + (1 - m_CurrentLightScatteringVolume)); }

private:
	XMUINT3 m_VolumeResolution = { 512, 288, 128 };
	XMUINT3 m_DispatchGroups;

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
	// Double-buffered to allow temporal integration
	std::array<Texture, 2> m_LightScatteringVolumes;
	UINT m_CurrentLightScatteringVolume = 0;

	Texture m_IntegratedVolume;

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

	VolumetricsConstantBuffer m_VolumeStagingBuffer;
	GlobalFogConstantBuffer m_GlobalFogStagingBuffer;
};
