#pragma once

#include "Core.h"

#include "D3DPipeline.h"
#include "Buffer/Texture.h"
#include "Lighting/VolumetricRendering.h"
#include "Memory/MemoryAllocator.h"


class MaterialManager;
class LightManager;
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
		GB_SRV_Depth0,
		GB_SRV_Depth1,
		GB_SRV_MAX
	};

public:
	DeferredRenderer();
	~DeferredRenderer();

	DISALLOW_COPY(DeferredRenderer)
	DEFAULT_MOVE(DeferredRenderer)


	void SetScene(const Scene& scene, LightManager& lightManager, const MaterialManager& materialManager);
	// Resize gbuffer resources to match the resolution of the back buffer
	void OnBackBufferResize();

	// Rendering

	// Perform g buffer population pass
	void Render();

	ID3D12Resource* GetOutputResource() const { return m_OutputResource.GetResource(); }
	ID3D12Resource* GetGBufferResource(UINT index) const { return m_RenderTargets.at(index).GetResource(); }

	ID3D12Resource* GetCurrentDepthBufferResource() const { return m_DepthBuffers.at(m_CurrentDepthBuffer).GetResource(); }
	ID3D12Resource* GetPreviousDepthBufferResource() const { return m_DepthBuffers.at(1 - m_CurrentDepthBuffer).GetResource(); }


	void DrawGui();

private:
	inline Texture& GetCurrentDepthBuffer() { return m_DepthBuffers.at(m_CurrentDepthBuffer); }
	inline Texture& GetPreviousDepthBuffer() { return m_DepthBuffers.at(1 - m_CurrentDepthBuffer); }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetCurrentDepthBufferSRV() const { return m_SRVs.GetGPUHandle(GB_SRV_Depth0 + m_CurrentDepthBuffer); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPreviousDepthBufferSRV() const { return m_SRVs.GetGPUHandle(GB_SRV_Depth0 + (1 - m_CurrentDepthBuffer)); }

private:
	void CreatePipelines();
	void CreateResolutionDependentResources();

	// Sub-stages in rendering pipeline
	void RenderShadowMaps() const;
	void CreateESM() const;
	
	void ClearGBuffer() const;
	void GeometryPass() const;
	void RenderSkybox() const;
	void LightingPass() const;

	void RenderVolumetrics() const;

	// Post processing
	void Tonemapping() const;

	// Submits Draw calls for all geometry in the scene
	void DrawAllGeometry(ID3D12GraphicsCommandList* commandList, UINT objectCBParamIndex) const;

private:
	// The scene to render
	const Scene* m_Scene = nullptr;

	LightManager* m_LightManager = nullptr;
	const MaterialManager* m_MaterialManager = nullptr;

	std::unique_ptr<VolumetricRendering> m_VolumeRenderer;
	bool m_UseVolumetrics = true;

	// G-buffer resources
	// The deferred renderer owns:
	// - a collection of render targets for various purposes
	// - a depth buffer

	UINT m_GBufferWidth;
	UINT m_GBufferHeight;

	// Previous frame dimensions
	UINT m_PrevGBufferWidth;
	UINT m_PrevGBufferHeight;

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

	constexpr inline static UINT64 s_DepthBufferCount = 2;
	std::array<Texture, s_DepthBufferCount> m_DepthBuffers;
	UINT m_CurrentDepthBuffer = 0;

	// A texture to hold the output of the lighting pass
	Texture m_OutputResource;

	DescriptorAllocation m_RTVs;
	DescriptorAllocation m_DSVs;
	DescriptorAllocation m_SRVs;

	// Output resource
	DescriptorAllocation m_OutputUAV;
	DescriptorAllocation m_OutputRTV;

	// Pipeline states
	D3DGraphicsPipeline m_DepthOnlyPipeline;

	D3DGraphicsPipeline m_GeometryPassPipeline;
	D3DComputePipeline m_LightingPipeline;

	D3DGraphicsPipeline m_SkyboxPipeline;

	D3DComputePipeline m_TonemappingPipeline;

	D3DComputePipeline m_ESMDownsamplePipeline;

	std::array<D3DComputePipeline, 2> m_SeparableBoxFilterPipelines;
};
