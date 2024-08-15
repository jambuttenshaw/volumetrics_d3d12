#pragma once

#include "Renderer/Buffer/Texture.h"
#include "Renderer/Memory/MemoryAllocator.h"


class ExponentialShadowMap
{
public:
	ExponentialShadowMap() = default;
	virtual ~ExponentialShadowMap();

	DISALLOW_COPY(ExponentialShadowMap)
	DEFAULT_MOVE(ExponentialShadowMap)

	void CreateExponentialShadowMap(const class ShadowMap& shadowMap);

	inline const XMUINT2& GetDimensions() const { return m_Dimensions; }

	inline ID3D12Resource* GetReadResource() const { return m_Textures.at(m_ReadIndex).GetResource(); }
	inline ID3D12Resource* GetWriteResource() const { return m_Textures.at(1 - m_ReadIndex).GetResource(); }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_Descriptors.GetGPUHandle(2 * m_ReadIndex); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const { return m_Descriptors.GetGPUHandle(3 - 2 * m_ReadIndex); }

	inline void FlipResources() { m_ReadIndex = 1 - m_ReadIndex; }

private:
	std::array<Texture, 2> m_Textures;	// Ping-pong resources as multiple passes of processing are required to build the ESM
	UINT m_ReadIndex = 0;

	XMUINT2 m_Dimensions;

	DescriptorAllocation m_Descriptors;
};
