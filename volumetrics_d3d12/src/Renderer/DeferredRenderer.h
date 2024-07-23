#pragma once
#include "Core.h"
#include "D3DPipeline.h"
#include "Buffer/Texture.h"


class Scene;


class DeferredRenderer
{
	enum GB_RTV
	{
		GB_RTV_Albedo = 0,
		GB_RTV_Normal,
		GB_RTV_RoughnessMetalness,
		GB_RTV_MAX
	};
	enum GB_SRV
	{
		GB_SRV_Albedo = 0,
		GB_SRV_Normal,
		GB_SRV_RoughnessMetalness,
		GB_SRV_Depth,
		GB_SRV_MAX
	};

public:
	DeferredRenderer();
	~DeferredRenderer();

	DISALLOW_COPY(DeferredRenderer)
	DEFAULT_MOVE(DeferredRenderer)


	void Setup(const Scene& scene);
	// Resize gbuffer resources to match the resolution of the back buffer
	void OnBackBufferResize();

private:
	// The scene to render
	const Scene* m_Scene = nullptr;

	// G-buffer resources
	// The deferred renderer owns:
	// - a collection of render targets for various purposes
	// - a depth buffer

	constexpr inline static UINT64 s_RTCount = 3; // The number of RTs in the gbuffer
	constexpr std::array<DXGI_FORMAT, s_RTCount> m_RTFormats = {
		DXGI_FORMAT_R11G11B10_FLOAT, // albedo
		DXGI_FORMAT_R32G32B32_FLOAT, // normal
		DXGI_FORMAT_R16G16_UNORM	 // roughness + metallic
	};
	constexpr DXGI_FORMAT m_DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// Resources
	std::array<Texture, s_RTCount> m_RenderTargets;
	Texture m_DepthBuffer;

	DescriptorAllocation m_RTVs;
	DescriptorAllocation m_DSV;
	DescriptorAllocation m_SRVs;

	// Pipeline state
	D3DGraphicsPipeline m_GBufferPipeline;

};
