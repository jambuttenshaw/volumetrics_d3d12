#pragma once
#include "Core.h"
#include "D3DPipeline.h"
#include "Buffer/Texture.h"


using namespace DirectX;

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

	// Rendering

	// Perform g buffer population pass
	void Render();

	ID3D12Resource* GetOutputResource() const { return m_LitOutput.GetResource(); }

private:
	void CreateResolutionDependentResources();

private:
	// The scene to render
	const Scene* m_Scene = nullptr;

	// G-buffer resources
	// The deferred renderer owns:
	// - a collection of render targets for various purposes
	// - a depth buffer

	constexpr inline static UINT64 s_RTCount = 3; // The number of RTs in the gbuffer
	std::array<DXGI_FORMAT, s_RTCount> m_RTFormats = {
		DXGI_FORMAT_R11G11B10_FLOAT, // albedo
		DXGI_FORMAT_R11G11B10_FLOAT, // normal
		DXGI_FORMAT_R16G16_UNORM	 // roughness + metallic
	};
	DXGI_FORMAT m_DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT m_OutputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Resources
	std::array<Texture, s_RTCount> m_RenderTargets;
	Texture m_DepthBuffer;
	// A texture to hold the output of the lighting pass
	Texture m_LitOutput;

	DescriptorAllocation m_RTVs;
	DescriptorAllocation m_DSV;
	DescriptorAllocation m_SRVs;
	DescriptorAllocation m_UAV;

	// Pipeline states
	D3DGraphicsPipeline m_GBufferPipeline;
	D3DComputePipeline m_LightingPipeline;

};
