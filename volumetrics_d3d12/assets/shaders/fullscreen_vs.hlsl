#ifndef FULLSCREENVS_HLSL
#define FULLSCREENVS_HLSL


#include "gbuffer/deferred_rendering.hlsli"


#define HLSL
#include "HlslCompat/StructureHlslCompat.h"


// Pass constant buffer for access to view and projection matrices
ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
ConstantBuffer<FullscreenQuadProperties> g_FullscreenQuadProperties : register(b1);


FullscreenVSOutputType main(uint id : SV_VertexID)
{
	FullscreenVSOutputType output;

	// Generate UV coordinate for a corner based on vertex ID
	output.UV = float2(id % 2, id / 2);
	// the UV coordinate can be mapped to a position in NDC space
	output.Position = float4((output.UV.x - 0.5f) * 2.0f, (0.5f - output.UV.y) * 2.0f, g_FullscreenQuadProperties.Depth, 1.0f);

	// Calculate view-space direction vector from camera through this vertex
	const float4 v = float4(
		output.Position.x / g_PassCB.Proj._11,
		output.Position.y / g_PassCB.Proj._22,
		1.0f,
		0.0f
	);
	// Transform into world-space
	output.WSViewDirection = mul(v, g_PassCB.InvView).xyz;

	return output;
}


#endif