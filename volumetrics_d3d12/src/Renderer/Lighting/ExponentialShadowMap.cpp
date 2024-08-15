#include "pch.h"
#include "ExponentialShadowMap.h"

#include "ShadowMap.h"
#include "Renderer/D3DGraphicsContext.h"


ExponentialShadowMap::~ExponentialShadowMap()
{
	m_Descriptors.Free();
}

void ExponentialShadowMap::CreateExponentialShadowMap(const ShadowMap& shadowMap)
{
	const auto dims = shadowMap.GetDimensions();
	ASSERT(dims.x % 4 == 0 && dims.y % 4 == 0, "Invalid shadow map resolution to create ESM!");

	m_Dimensions = { dims.x / 4, dims.y / 4 };

	auto desc = CD3DX12_RESOURCE_DESC::Tex2D(ShadowMap::GetSRVFormat(), m_Dimensions.x, m_Dimensions.y);
	desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	for (auto& texture : m_Textures)
	{
		texture.Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	// Create descriptors
	m_Descriptors = g_D3DGraphicsContext->GetSRVHeap()->Allocate(4);
	ASSERT(m_Descriptors.IsValid(), "Failed to alloc!");

	UINT i = 0;
	for (const auto& texture : m_Textures)
	{
		g_D3DGraphicsContext->GetDevice()->CreateShaderResourceView(texture.GetResource(), nullptr, m_Descriptors.GetCPUHandle(i++));
		g_D3DGraphicsContext->GetDevice()->CreateUnorderedAccessView(texture.GetResource(), nullptr, nullptr, m_Descriptors.GetCPUHandle(i++));
	}
}
