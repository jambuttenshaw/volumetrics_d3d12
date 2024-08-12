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


// Shadowing
float GetVisibility(float3 worldPos)
{
	float4 shadowPos = mul(float4(worldPos, 1.0f), g_LightCB.DirectionalLight.ViewProjection);
	shadowPos /= shadowPos.w;

	float2 shadowMapUV = shadowPos.xy * 0.5f + 0.5f;
	shadowMapUV.y = 1.0f - shadowMapUV.y;

	return g_SunShadowMap.SampleCmpLevelZero(g_ShadowMapSampler, shadowMapUV, shadowPos.z);
}



[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Load values from volume buffer
	const float4 vba = g_VBufferA[DTid];
	const float4 vbb = g_VBufferB[DTid];

	const float3 scattering = vba.rgb;
	const float extinction = vba.a;

	const float3 emission = vbb.rgb;
	const float anisotropy = vbb.a;

	// Get location of this froxel in world space
	const float depth = ZSliceToFroxelDepth(DTid.z, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance, g_VolumeCB.VolumeResolution.z);

	// Need to get the depth in NDC
	float4 depthNDC = mul(float4(0.0f, 0.0f, depth, 1.0f), g_PassCB.Proj);
	depthNDC /= depthNDC.w;

	// we want to calculate at the center of each froxel
	float2 sliceUV = (DTid.xy + float2(0.5f, 0.5f)) / (float2) (g_VolumeCB.VolumeResolution.xy);
	float2 sliceNDC = (2.0f * sliceUV - 1.0f) * float2(1.0f, -1.0f);

	float4 p_vs = mul(float4(sliceNDC, depthNDC.z, 1.0f), g_PassCB.InvProj);
	p_vs /= p_vs.w;
	float4 p_ws = mul(p_vs, g_PassCB.InvView);

	float3 in_scattering = emission; // add emission to light scattered into the cameras path at this location

	// Evaluate in-scattering from directional light
	{
		const float3 l = -normalize(g_LightCB.DirectionalLight.Direction);
		const float3 el = g_LightCB.DirectionalLight.Intensity * g_LightCB.DirectionalLight.Color;
		const float3 v = normalize(g_PassCB.WorldEyePos - p_ws.xyz);

		const float phase = HGPhaseFunction(v, l, anisotropy);
		const float visibility = GetVisibility(p_ws.xyz); // Sample shadow map to get visibility

		in_scattering += phase * visibility * el;
	}

	const float3 l_scat = in_scattering * scattering;

	g_LightScatteringVolume[DTid] = float4(l_scat, extinction);
}

#endif