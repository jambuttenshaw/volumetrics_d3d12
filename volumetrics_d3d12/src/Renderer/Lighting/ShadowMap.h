#pragma once


#include "Renderer/Buffer/Texture.h"
#include "Renderer/Memory/MemoryAllocator.h"

using namespace DirectX;


class ShadowMap
{
public:
	ShadowMap() = default;
	ShadowMap(UINT width, UINT height, UINT arraySize = 1);
	virtual ~ShadowMap();

	DISALLOW_COPY(ShadowMap);
	DEFAULT_MOVE(ShadowMap);

	void CreateShadowMap(UINT width, UINT height, UINT arraySize = 1);

	inline const XMUINT2& GetDimensions() const { return m_Dimensions; }
	inline ID3D12Resource* GetResource() const { return m_Texture.GetResource(); }

	inline D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DSV.GetCPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_SRV.GetGPUHandle(); }

	void ApplyViewport(ID3D12GraphicsCommandList* commandList) const;

	static constexpr DXGI_FORMAT GetDSVFormat() { return s_DSVFormat; }
	static constexpr DXGI_FORMAT GetSRVFormat() { return s_SRVFormat; }

private:
	XMUINT2 m_Dimensions;
	Texture m_Texture;

	static constexpr DXGI_FORMAT s_DSVFormat = DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT s_SRVFormat = DXGI_FORMAT_R32_FLOAT;

	DescriptorAllocation m_DSV;
	DescriptorAllocation m_SRV;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;
};
