#pragma once

#include "Core.h"
#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/Texture.h"
#include "Renderer/Memory/MemoryAllocator.h"


class IBL
{
	enum IBLPipelines
	{
		IBL_Pipeline_IrradianceMap = 0,
		IBL_Pipeline_BRDFIntegration,
		IBL_Pipeline_PreFilteredEnvironmentMap,
		IBL_Pipeline_Count
	};

	// This only contains SRVs so that all global lighting descriptors can be contained within one table
	enum GlobalLightingSRV
	{
		EnvironmentMapSRV = 0,
		IrradianceMapSRV,
		BRDFIntegrationMapSRV,
		PreFilteredEnvironmentMapSRV,
		SRVCount
	};

	enum Samplers
	{
		EnvironmentMapSampler = 0,
		BRDFIntegrationMapSampler,
		SamplerCount
	};

	template<size_t Order>
	struct SkyIrradianceSH
	{
		// A set of SH Coefficients for each channel to represent sky irradiance
		std::array<float, Order * Order> R;
		std::array<float, Order * Order> G;
		std::array<float, Order * Order> B;
	};

public:
	IBL();
	~IBL();

	DISALLOW_COPY(IBL)
	DEFAULT_MOVE(IBL)

	// These methods are REALLY SLOW - only call once
	void ProcessEnvironmentMap(std::unique_ptr<Texture>&& map);
	void ProjectIrradianceMapToSH();

	void PopulateSkyIrradianceSHConstants(XMFLOAT4* OutConstants) const;

	// Get resources
	inline Texture* GetEnvironmentMap() const { return m_EnvironmentMap.get(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapSRV() const { return m_GlobalLightingSRVs.GetGPUHandle(EnvironmentMapSRV); }

	// Get descriptors
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRVTable() const { return m_GlobalLightingSRVs.GetGPUHandle(IrradianceMapSRV); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerTable() const { return m_SamplerDescriptors.GetGPUHandle(); }


private:
	void CreatePipelines();
	void CreateResources();

private:
	// Environmental lighting resources
	std::unique_ptr<Texture> m_EnvironmentMap;

	// Quality controls for environment map
	UINT m_IrradianceMapResolution = 32;
	UINT m_BRDFIntegrationMapResolution = 512;
	UINT m_PEMResolution = 128;
	UINT m_PEMRoughnessBins = 5;

	// Pre-processed image-based lighting resources
	std::unique_ptr<Texture> m_IrradianceMap;
	std::unique_ptr<Texture> m_BRDFIntegrationMap;
	std::unique_ptr<Texture> m_PreFilteredEnvironmentMap;

	// Spherical harmonic representation of sky irradiance
	SkyIrradianceSH<3> m_SkyIrradianceSH;

	// All descriptors for global lighting
	DescriptorAllocation m_GlobalLightingSRVs;
	DescriptorAllocation m_SamplerDescriptors;

	// API resources to process the environment
	// As this is not an operation that occurs often it will just block the render queue to process the environment map
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;
	std::array<D3DComputePipeline, IBL_Pipeline_Count> m_Pipelines;

	UINT64 m_PreviousWorkFence = 0;
};
