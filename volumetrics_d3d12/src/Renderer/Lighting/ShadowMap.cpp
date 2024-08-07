#include "pch.h"
#include "ShadowMap.h"

#include "Renderer/D3DGraphicsContext.h"


ShadowMap::ShadowMap(UINT width, UINT height, UINT arraySize)
{
	CreateShadowMap(width, height, arraySize);
}

ShadowMap::~ShadowMap()
{
	m_DSV.Free();
	m_SRV.Free();
}


void ShadowMap::CreateShadowMap(UINT width, UINT height, UINT arraySize)
{
	const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(s_DSVFormat, width, height, arraySize, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = s_DSVFormat;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, &clearValue);


	// Create DSV
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Format = s_DSVFormat;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		if (arraySize > 1)
		{
			// TODO: Need a separate DSV for each array slice
			NOT_IMPLEMENTED;
		}
		else
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
		}

		m_DSV = g_D3DGraphicsContext->GetDSVHeap()->Allocate(1);
		ASSERT(m_DSV.IsValid(), "Failed to alloc");
		g_D3DGraphicsContext->GetDevice()->CreateDepthStencilView(GetResource(), &dsvDesc, m_DSV.GetCPUHandle());
	}

	// Create SRV
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = s_SRVFormat;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (arraySize > 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = arraySize;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		m_SRV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
		ASSERT(m_SRV.IsValid(), "Failed to alloc");
		g_D3DGraphicsContext->GetDevice()->CreateShaderResourceView(GetResource(), &srvDesc, m_SRV.GetCPUHandle());
	}

	m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	m_ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
}


void ShadowMap::ApplyViewport(ID3D12GraphicsCommandList* commandList) const
{
	commandList->RSSetViewports(1, &m_Viewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);
}
