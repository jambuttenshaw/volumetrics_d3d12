#pragma once

#include "Core.h"
#include "ShadowMap.h"
#include "HlslCompat/StructureHlslCompat.h"
#include "Renderer/D3DPipeline.h"

#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Memory/MemoryAllocator.h"
#include "ExponentialShadowMap.h"

class IBL;

using namespace DirectX;


class LightManager
{
public:
	enum ShadowSampler
	{
		ShadowSampler_Comparison = 0,
		ShadowSampler_ESM,
		ShadowSampler_Count
	};

public:
	LightManager();
	~LightManager();

	DISALLOW_COPY(LightManager);
	DEFAULT_MOVE(LightManager);

	void UpdateLightingCB(const XMFLOAT3& eyePos);

	ShadowMap& GetSunShadowMap() { return m_SunShadowMap; }
	ExponentialShadowMap& GetSunESM() { return m_SunESM; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetShadowSampler(ShadowSampler sampler) const { return m_ShadowSamplers.GetGPUHandle(sampler); }

	// Call each frame to move the latest lighting data to the GPU
	void CopyStagingBuffers() const;

	D3D12_GPU_VIRTUAL_ADDRESS GetLightingConstantBuffer() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetPointLightBuffer() const;

	inline UINT GetLightCount() const { return s_MaxLights; }

	IBL* GetIBL() const { return m_IBL.get(); }

	void DrawGui();


private:
	inline static constexpr size_t s_MaxLights = 1;

	LightingConstantBuffer m_LightingCBStaging;
	std::array<PointLightGPUData, s_MaxLights> m_PointLightsStaging;

	XMMATRIX m_ShadowCameraProjectionMatrix;
	ShadowMap m_SunShadowMap;	// Shadow map for the directional light
	ExponentialShadowMap m_SunESM;

	// Light GPU Resources
	// This is a buffered resource so that light data can be modified between frames
	struct LightingGPUResourcesPerFrame
	{
		UploadBuffer<LightingConstantBuffer> LightingCB;
		UploadBuffer<PointLightGPUData> PointLights;
	};
	std::vector<LightingGPUResourcesPerFrame> m_LightingResources;

	DescriptorAllocation m_ShadowSamplers;

	std::unique_ptr<IBL> m_IBL;
};
