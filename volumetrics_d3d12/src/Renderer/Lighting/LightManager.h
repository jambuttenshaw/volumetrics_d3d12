#pragma once

#include "Core.h"
#include "ShadowMap.h"
#include "HlslCompat/StructureHlslCompat.h"

#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Memory/MemoryAllocator.h"

class IBL;

using namespace DirectX;


class LightManager
{
public:
	LightManager();
	~LightManager();

	DISALLOW_COPY(LightManager);
	DEFAULT_MOVE(LightManager);

	void UpdateLightingCB(const XMFLOAT3& eyePos);

	const ShadowMap& GetSunShadowMap() const { return m_SunShadowMap; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetShadowSampler() const { return m_ShadowSampler.GetGPUHandle(); }

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

	// Light GPU Resources
	// This is a buffered resource so that light data can be modified between frames
	struct LightingGPUResourcesPerFrame
	{
		UploadBuffer<LightingConstantBuffer> LightingCB;
		UploadBuffer<PointLightGPUData> PointLights;
	};
	std::vector<LightingGPUResourcesPerFrame> m_LightingResources;

	DescriptorAllocation m_ShadowSampler;

	std::unique_ptr<IBL> m_IBL;
};
