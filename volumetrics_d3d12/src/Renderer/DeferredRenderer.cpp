#include "pch.h"
#include "DeferredRenderer.h"

#include "D3DGraphicsContext.h"


DeferredRenderer::DeferredRenderer()
{
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

}

DeferredRenderer::~DeferredRenderer()
{
	
}

void DeferredRenderer::Setup(const Scene& scene)
{
	
}


