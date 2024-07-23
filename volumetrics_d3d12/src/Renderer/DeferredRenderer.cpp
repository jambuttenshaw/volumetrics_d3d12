#include "pch.h"
#include "DeferredRenderer.h"

#include "D3DGraphicsContext.h"
#include "Application/Scene.h"


DeferredRenderer::DeferredRenderer()
{
	CreateResolutionDependentResources();

	// Create g buffer pipeline state
	{
		D3DGraphicsPipelineDesc psoDesc = {};

		// Set up root parameters

		// Shaders
		psoDesc.VertexShader.ShaderPath = L"assets/shaders/gbuffer/basic_vs.hlsl";
		psoDesc.VertexShader.EntryPoint = L"main";

		psoDesc.PixelShader.ShaderPath = L"assets/shaders/gbuffer/basic_ps.hlsl";
		psoDesc.PixelShader.EntryPoint = L"main";

		// Add all render target formats to the description
		psoDesc.RenderTargetFormats.insert(psoDesc.RenderTargetFormats.end(), m_RTFormats.begin(), m_RTFormats.end());

		m_GBufferPipeline.Create(&psoDesc);
	}

	// Create lighting pass pipeline state
	{
		D3DComputePipelineDesc psoDesc = {};

		// Set up root parameters

		psoDesc.Shader = L"assets/shaders/lighting/lighting_pass_cs.hlsl";
		psoDesc.EntryPoint = L"main";

		m_LightingPipeline.Create(&psoDesc);
	}
}

DeferredRenderer::~DeferredRenderer()
{
	// Free descriptors
	m_RTVs.Free();
	m_DSV.Free();
	m_SRVs.Free();
}

void DeferredRenderer::OnBackBufferResize()
{
	m_RTVs.Free();
	m_DSV.Free();
	m_SRVs.Free();

	CreateResolutionDependentResources();
}


void DeferredRenderer::CreateResolutionDependentResources()
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

	for (UINT rt = 0; rt < s_RTCount; rt++)
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

		for (UINT rt = 0; rt < s_RTCount; rt++)
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

		for (UINT rt = 0; rt < s_RTCount; rt++)
		{
			srvDesc.Format = m_RTFormats.at(rt);
			device->CreateShaderResourceView(m_RenderTargets.at(rt).GetResource(), &srvDesc, m_SRVs.GetCPUHandle(rt));
		}

		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		device->CreateShaderResourceView(m_DepthBuffer.GetResource(), &srvDesc, m_SRVs.GetCPUHandle(GB_SRV_Depth));
	}
}

void DeferredRenderer::Setup(const Scene& scene)
{
	m_Scene = &scene;
}


void DeferredRenderer::RenderGeometryBuffer()
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	// Clear g buffer
	for (UINT rt = 0; rt < s_RTCount; rt++)
	{
		constexpr XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		commandList->ClearRenderTargetView(m_RTVs.GetCPUHandle(rt), &clearColor.x, 0, nullptr);
	}
	commandList->ClearDepthStencilView(m_DSV.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Assign render targets and depth buffer
	const auto firstRTV = m_RTVs.GetCPUHandle();
	const auto dsv = m_DSV.GetCPUHandle();
	commandList->OMSetRenderTargets(s_RTCount, &firstRTV, true, &dsv);


	// Perform drawing into g-buffer
	m_GBufferPipeline.Bind(commandList);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (const auto& geometryInstance : m_Scene->GetAllGeometryInstances())
	{
		// Set material root parameters
		const TriangleGeometry* geometry = geometryInstance.GetGeometry();

		commandList->IASetIndexBuffer(&geometry->IndexBufferView);
		commandList->IASetVertexBuffers(0, 1, &geometry->VertexBufferView);

		commandList->DrawIndexedInstanced(geometry->IndexBuffer.GetElementCount(), 1, 0, 0, 0);
	}

	// Switch resource states
	{
		
	}

	// Perform lighting pass

}

