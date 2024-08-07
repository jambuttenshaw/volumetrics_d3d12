#pragma once


#include "Renderer/Buffer/Texture.h"
#include "Renderer/Memory/MemoryAllocator.h"


class ShadowMap : public Texture
{
public:
	ShadowMap() = default;
	ShadowMap(UINT width, UINT height, UINT arraySize = 1);
	virtual ~ShadowMap();

	DISALLOW_COPY(ShadowMap);
	DEFAULT_MOVE(ShadowMap);

	void CreateShadowMap(UINT width, UINT height, UINT arraySize = 1);


	inline D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DSV.GetCPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_SRV.GetGPUHandle(); }

	void ApplyViewport(ID3D12GraphicsCommandList* commandList) const;

private:
	static constexpr DXGI_FORMAT s_DSVFormat = DXGI_FORMAT_D16_UNORM;
	static constexpr DXGI_FORMAT s_SRVFormat = DXGI_FORMAT_R16_UNORM;

	DescriptorAllocation m_DSV;
	DescriptorAllocation m_SRV;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;
};
