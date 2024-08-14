#pragma once

#include "Renderer/Buffer/Texture.h"
#include "Renderer/Memory/MemoryAllocator.h"


class ExponentialShadowMap : public Texture
{
public:
	ExponentialShadowMap() = default;
	virtual ~ExponentialShadowMap();

	DISALLOW_COPY(ExponentialShadowMap)
	DEFAULT_MOVE(ExponentialShadowMap)

	void CreateExponentialShadowMap(const class ShadowMap& shadowMap);

	inline const XMUINT2& GetDimensions() const { return m_Dimensions; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_Descriptors.GetGPUHandle(0); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const { return m_Descriptors.GetGPUHandle(1); }

private:
	XMUINT2 m_Dimensions;
	DescriptorAllocation m_Descriptors;
};
