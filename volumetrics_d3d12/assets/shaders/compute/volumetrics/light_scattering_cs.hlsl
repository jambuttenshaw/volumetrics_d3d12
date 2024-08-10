#ifndef LIGHTSCATTERINGCS_HLSL
#define LIGHTSCATTERINGCS_HLSL


#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

#include "volumetrics.hlsli"


ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
ConstantBuffer<LightingConstantBuffer> g_LightCB : register(b1);

ConstantBuffer<VolumetricsConstantBuffer> g_VolumeCB : register(b2);

/*
 * Format: RGBA16F
 * RGB - Scattering
 * A   - Extinction
*/
Texture3D<float4> g_VBufferA : register(t0);
/*
 * Format: RGBA16F
 * RGB - Emissive
 * A   - Phase (g)
*/
Texture3D<float4> g_VBufferB : register(t1);


Texture2D<float> g_SunShadowMap : register(t2);


SamplerComparisonState g_ShadowMapSampler : register(s0);


RWTexture3D<float4> g_LightScatteringVolume : register(u0);


// Froxel helper functions
float ZSliceToFroxelDepth(uint slice)
{
	const float sliceNormalized = (float) (slice) / (float) (g_VolumeCB.VolumeResolution.z - 1);
	return g_PassCB.NearPlane - 1 + (1 + g_VolumeCB.MaxVolumeDistance - g_PassCB.NearPlane) * exp2(sliceNormalized);
}


// Shadowing
float GetVisibility(float3 worldPos)
{
	float4 shadowPos = mul(worldPos, g_LightCB.DirectionalLight.ViewProjection);
	shadowPos /= shadowPos.w;

	float2 shadowMapUV = shadowPos.xy * 0.5f + 0.5f;
	shadowMapUV.y = 1.0f - shadowMapUV.y;

	return g_SunShadowMap.SampleCmpLevelZero(g_ShadowMapSampler, shadowMapUV, shadowPos.z);
}



[numthreads(8, 8, 8)]
void main(uint3 DTid : S_DispatchThreadID)
{
	// Load values from volume buffer
	const float4 vba = g_VBufferA[DTid];
	const float4 vbb = g_VBufferB[DTid];

	const float3 scattering = vba.rgb;
	const float extinction = vba.a;

	const float3 emission = vbb.rgb;
	const float anisotropy = vbb.a;

	const float3 albedo = scattering / extinction;

	// Get location of this froxel in world space
	const float depth = ZSliceToFroxelDepth(DTid.z);

	// we want to calculate at the center of each froxel
	float2 sliceUV = (DTid.xy + float2(0.5f, 0.5f)) / (float2) (g_VolumeCB.VolumeResolution.xy);

	// v in view space
	const float3 v_vs = normalize(float3(sliceUV.x / g_PassCB.Proj._11, sliceUV.y / g_PassCB.Proj._22, 1.0f));

	// froxel centre in view space
	const float3 p_vs = v_vs * depth;

	const float3 v_ws = mul(v_vs, g_PassCB.InvView);
	const float3 p_ws = mul(p_vs, g_PassCB.InvView);

	float3 in_scattering = float3(0.0f, 0.0f, 0.0f);

	// Evaluate in-scattering from directional light
	{
		const float3 l = -g_LightCB.DirectionalLight.Direction;
		const float3 el = g_LightCB.DirectionalLight.Intensity * g_LightCB.DirectionalLight.Color;
		const float3 v = -v_vs;

		const float phase = HGPhaseFunction(v, l, anisotropy);
		const float visibility = GetVisibility(p_ws); // Sample shadow map to get visibility

		in_scattering += phase * visibility * el;
	}

	const float3 l_scat = albedo * in_scattering;

	g_LightScatteringVolume[DTid] = float4(l_scat, extinction);
}

#endif