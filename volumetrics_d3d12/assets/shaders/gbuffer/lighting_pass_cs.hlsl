#ifndef LIGHTINGPASSCS_HLSL
#define LIGHTINGPASSCS_HLSL

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "../include/lighting_helper.hlsli"
#include "../include/lighting_ibl.hlsli"


ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0, space0);

// G-buffer resources
Texture2D<float3> g_Albedo : register(t0, space0);
Texture2D<float3> g_Normal : register(t1, space0);
Texture2D<float2> g_RoughnessMetallic : register(t2, space0);
Texture2D<float> g_Depth : register(t3, space0);

// Scene lighting
StructuredBuffer<LightGPUData> g_SceneLights : register(t4, space0);

// Environmental lighting resources
TextureCube g_IrradianceMap : register(t0, space1);
Texture2D g_BRDFIntegrationMap : register(t1, space1); 
TextureCube g_PrefilteredEnvironmentMap : register(t2, space1);

SamplerState g_EnvironmentSampler : register(s0, space1);
SamplerState g_BRDFIntegrationSampler : register(s1, space1);
 
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

	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, albedo, roughnessMetallic.y);

	float3 lo = float3(0.0f, 0.0f, 0.0f);

	for (uint i = 0; i < g_PassCB.LightCount; i++)
	{
		// Get lighting parameters
		const LightGPUData light = g_SceneLights[i];
		float3 l = -light.Direction;
		float3 el = light.Color * light.Intensity;

		// evaluate shading equation
		float3 brdf = ggx_brdf(v, l, normal, albedo, f0, roughnessMetallic.x, roughnessMetallic.y) * el * saturate(dot(normal, l));

		lo += brdf;
	}

	// Calculate ambient lighting
	const float3 ambient = calculateAmbientLighting(
		normal,
		v,
		albedo,
		f0,
		roughnessMetallic.x, 
		roughnessMetallic.y,
		g_IrradianceMap,
		g_BRDFIntegrationMap,
		g_PrefilteredEnvironmentMap,
		g_EnvironmentSampler,
		g_BRDFIntegrationSampler
	);

	lo += ambient;

	// Gamma correction
	float3 color = pow(lo, 0.4545f);

	g_LitOutput[DTid.xy] = float4(color, 1.0f);

}

#endif
