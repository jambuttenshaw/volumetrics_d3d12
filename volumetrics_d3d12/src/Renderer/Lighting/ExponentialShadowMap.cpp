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

	Allocate(&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Create descriptors
	m_Descriptors = g_D3DGraphicsContext->GetSRVHeap()->Allocate(2);
	ASSERT(m_Descriptors.IsValid(), "Failed to alloc!");

	g_D3DGraphicsContext->GetDevice()->CreateShaderResourceView(GetResource(), nullptr, m_Descriptors.GetCPUHandle(0));
	g_D3DGraphicsContext->GetDevice()->CreateUnorderedAccessView(GetResource(), nullptr, nullptr, m_Descriptors.GetCPUHandle(1));
}
