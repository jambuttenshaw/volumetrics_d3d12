#ifndef LIGHTINGPASSCS_HLSL
#define LIGHTINGPASSCS_HLSL

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "../include/lighting_helper.hlsli"

ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);

// G-buffer resources
Texture2D<float3> g_Albedo : register(t0);
Texture2D<float3> g_Normal : register(t1);
Texture2D<float2> g_RoughnessMetallic : register(t2);
Texture2D<float> g_Depth : register(t3);
 
// Lighting output
RWTexture2D<float4> g_LitOutput : register(u0);


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 dims;
	g_LitOutput.GetDimensions(dims.x, dims.y);
	if (DTid.x >= dims.x || DTid.y >= dims.y)
	{
		return;
	}

	// Recover NDC coordinates from thread ID
	float2 uv = (DTid.xy + float2(0.5f, 0.5f)) / float2(dims.xy);
	uv.y = 1.0f - uv.y;
	float2 ndc = 2.0f * uv - 1.0f;

	// Get depth from buffer
	float depth = g_Depth[DTid.xy];
	if (depth >= 0.9999f)
	{
		return;
	}

	// Un-project to world-space
	float4 worldPos = mul(float4(ndc, depth, 1.0f), g_PassCB.InvViewProj);
	worldPos = worldPos / worldPos.w;

	// get view vector
	float3 v = normalize(g_PassCB.WorldEyePos - worldPos.xyz);

	// Shading time!
	float3 albedo = g_Albedo[DTid.xy];
	float3 normal = g_Normal[DTid.xy] * 2.0f - 1.0f;
	float2 roughnessMetallic = g_RoughnessMetallic[DTid.xy];

	float3 lightPos = float3(2.0f, 1.5f, -1.2f);

	float3 l = normalize(lightPos - worldPos.xyz);
	float3 el = float3(2.0f, 2.0f, 2.0f);
	float d = length(lightPos - worldPos.xyz);


	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, albedo, roughnessMetallic.y);

	// evaluate shading equation
	float3 brdf = ggx_brdf(v, l, normal, albedo, f0, roughnessMetallic.x, roughnessMetallic.y) * el * saturate(dot(normal, l));
	brdf /= (1.0f + 0.1f * d + 0.01f * d);

	float3 ambient = float3(0.01f, 0.01f, 0.01f);
	brdf += ambient;

	float3 color = pow(brdf, 0.4545f);

	g_LitOutput[DTid.xy] = float4(color, 1.0f);

}

#endif
