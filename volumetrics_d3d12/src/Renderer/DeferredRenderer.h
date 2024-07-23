#pragma once
#include "Core.h"
#include "D3DPipeline.h"
#include "Buffer/Texture.h"


class Scene;


class DeferredRenderer
{
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
	const Scene* m_Scene;

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

	// Pipeline state
	D3DGraphicsPipeline m_GBufferPipeline;

};
