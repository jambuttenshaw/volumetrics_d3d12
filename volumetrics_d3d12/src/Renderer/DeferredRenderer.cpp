#include "pch.h"
#include "DeferredRenderer.h"

#include "D3DGraphicsContext.h"


DeferredRenderer::DeferredRenderer()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Allocate render targets
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = g_D3DGraphicsContext->GetClientWidth();
	desc.Height = g_D3DGraphicsContext->GetClientHeight();
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	for (UINT64 rt = 0; rt < s_RTCount; rt++)
	{
		desc.Format = m_RTFormats[rt];
		m_RenderTargets[rt].Allocate(&desc, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	// Allocate depth buffer
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Format = m_DSVFormat;
	m_DepthBuffer.Allocate(&desc, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// Create RTVs and DSVs
	m_RTVs = g_D3DGraphicsContext->GetRTVHeap()->Allocate(s_RTCount);
	ASSERT(m_RTVs.IsValid(), "Failed to alloc!");

	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		for (UINT64 rt = 0; rt < s_RTCount; rt++)
		{
			rtvDesc.Format = m_RTFormats[rt];
			device->CreateRenderTargetView(m_RenderTargets.at(rt).GetResource(), &rtvDesc, m_RTVs.GetCPUHandle(rt));
		}
	}

	m_DSV = g_D3DGraphicsContext->GetDSVHeap()->Allocate(1);
	ASSERT(m_DSV.IsValid(), "Failed to alloc!");

	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_DSVFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(m_DepthBuffer.GetResource(), &dsvDesc, m_DSV.GetCPUHandle());
	}

	// Create SRVs
	m_SRVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(s_RTCount + 1);
	ASSERT(m_SRVs.IsValid(), "Failed to alloc!");

	{
		// Create SRVs for render targets
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		for (UINT64 rt = 0; rt < s_RTCount; rt++)
		{
			srvDesc.Format = m_RTFormats.at(rt);
			device->CreateShaderResourceView(m_RenderTargets.at(rt).GetResource(), &srvDesc, m_SRVs.GetCPUHandle(rt));
		}

		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		device->CreateShaderResourceView(m_DepthBuffer.GetResource(), &srvDesc, m_SRVs.GetCPUHandle(GB_SRV_Depth));
	}


	// Create pipeline state
	{
		D3DGraphicsPipelineDesc psoDesc = {};
	}
}

DeferredRenderer::~DeferredRenderer()
{
	// Free
	ASSERT(false, "Remember to free descriptors!");
}

void DeferredRenderer::Setup(const Scene& scene)
{
	
}


