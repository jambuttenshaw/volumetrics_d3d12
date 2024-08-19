#ifndef LIGHTSCATTERINGCS_HLSL
#define LIGHTSCATTERINGCS_HLSL


#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

#include "volumetrics.hlsli"

#include "../../include/lighting_helper.hlsli"
#include "../../include/random.hlsli"


ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
ConstantBuffer<LightingConstantBuffer> g_LightCB : register(b1);

ConstantBuffer<VolumetricsConstantBuffer> g_VolumeCB : register(b2);
ConstantBuffer<LightScatteringConstantBuffer> g_LightScatteringCB : register(b3);

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


// Scene Lighting resources
StructuredBuffer<PointLightGPUData> g_PointLights : register(t2);
Texture2D<float> g_SunSM : register(t3);

Texture2D<float> g_SceneDepth : register(t4);

SamplerComparisonState g_SMSampler : register(s0);
SamplerState g_ESMSampler : register(s1);


RWTexture3D<float4> g_LightScatteringVolume : register(u0);

// Previous resources for temporal reprojection
Texture3D<float4> g_PrevLightScatteringVolume : register(t0, space1);
Texture2D<float> g_PrevSceneDepth : register(t1, space1);

SamplerState g_HistoryVolumeSampler : register(s0, space1);


float3 ComputeWorldSpacePositionFromFroxelIndex(uint3 froxel, float3 cellOffset)
{
	const float depth = ZSliceToFroxelDepth(froxel.z + cellOffset.z, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance, g_VolumeCB.VolumeResolution.z);

	// Need to get the depth in NDC
	const float depthNDC = g_PassCB.ViewDepthToNDC.x - g_PassCB.ViewDepthToNDC.y / depth;

	// we want to calculate at the center of each froxel
	const float2 sliceUV = (froxel.xy + cellOffset.xy) / (float2) (g_VolumeCB.VolumeResolution.xy);
	float2 sliceNDC = (2.0f * sliceUV - 1.0f) * float2(1.0f, -1.0f);

	float4 p_vs = mul(float4(sliceNDC, depthNDC, 1.0f), g_PassCB.InvProj);
	p_vs /= p_vs.w;
	return mul(p_vs, g_PassCB.InvView).xyz;
}

float3 ComputeHistoryVolumeUVW(float3 p_ws)
{
	const float4 p_vs = mul(float4(p_ws, 1.0f), g_PassCB.PrevView);
	float4 ndc = mul(p_vs, g_PassCB.PrevProj);
	ndc /= ndc.w;

	float3 volumeUVW = float3(ndc.xy * float2(0.5f, -0.5f) + 0.5f,
							  FroxelDepthToVolumeW(p_vs.z, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance));
	return volumeUVW;
}


// Shadowing
float GetVisibility(float3 worldPos)
{
	float4 shadowPos = mul(float4(worldPos, 1.0f), g_LightCB.DirectionalLight.ViewProjection);
	shadowPos /= shadowPos.w;

	float2 shadowMapUV = shadowPos.xy * 0.5f + 0.5f;
	shadowMapUV.y = 1.0f - shadowMapUV.y;

	return g_SunSM.SampleCmpLevelZero(g_SMSampler, shadowMapUV, shadowPos.z);
}

float GetVisibility_ESM(float3 worldPos)
{
	float4 shadowPos = mul(float4(worldPos, 1.0f), g_LightCB.DirectionalLight.ViewProjection);
	shadowPos /= shadowPos.w;

	float2 shadowMapUV = shadowPos.xy * 0.5f + 0.5f;
	shadowMapUV.y = 1.0f - shadowMapUV.y;

	const float receiver = exp(shadowPos.z * ESM_EXPONENT);
	const float occluder = g_SunSM.SampleLevel(g_ESMSampler, shadowMapUV, 0);
	return 1.0f - saturate(receiver / occluder);
}


[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Check cell occlusion
	bool cellWasOccluded = false;
	{
		/*
		const float3 farDepthOffset = float3(0.5f, 0.5f, -1.0f);
		const float3 cellWorldPos = ComputeWorldSpacePositionFromFroxelIndex(DTid, farDepthOffset);

		float4 cellNDC = mul(float4(cellWorldPos, 1.0f), g_PassCB.ViewProj);
		cellNDC /= cellNDC.w;
		const float2 UVs = cellNDC.xy * float2(0.5f, -0.5f) + 0.5f;

		const float sceneDepth = g_SceneDepth.Load(uint3(UVs * g_LightScatteringCB.DepthBufferDimensions, 0));

		if (sceneDepth < cellNDC.z)
		{
			g_LightScatteringVolume[DTid] = float4(0.0f, 0.0f, 0.0f, 0.0f);
			return;
		}

		// Sample previous depth buffer to determine if this froxel was previously occluded
		float4 prevCellNDC = mul(float4(cellWorldPos, 1.0f), g_PassCB.PrevViewProj);
		prevCellNDC /= prevCellNDC.w;
		const float2 prevUVs = prevCellNDC.xy * float2(0.5f, -0.5f) + 0.5f;

		const float prevSceneDepth = g_PrevSceneDepth.Load(uint3(prevUVs * g_LightScatteringCB.PrevDepthBufferDimensions, 0));
		if (prevSceneDepth < prevCellNDC.z)
		{
			cellWasOccluded = true;
		}
		*/
	}

	// Load values from volume buffer
	const float4 vba = g_VBufferA[DTid];
	const float4 vbb = g_VBufferB[DTid];

	const float3 scattering = vba.rgb;
	float extinction = vba.a;

	const float3 emission = vbb.rgb;
	const float anisotropy = vbb.a;

	// Apply jitter to cell offset
	float3 cellOffset = float3(0.5f, 0.5f, 0.5f);

	if (g_VolumeCB.UseTemporalReprojection)
	{
		const uint3 rand32Bits = Rand4DPCG32(int4(DTid, g_PassCB.FrameIndexMod16)).xyz;
		const float3 rand3D = (float3(rand32Bits) / float(uint(0xffffffff))) - 0.5f;
		cellOffset = g_VolumeCB.LightScatteringJitterMultiplier * rand3D;
	}

	// Get location of this froxel in world space
	const float3 p_ws = ComputeWorldSpacePositionFromFroxelIndex(DTid, cellOffset);
	const float3 v = normalize(p_ws - g_PassCB.WorldEyePos);

	float historyAlpha = 0.0f;
	float3 historyUV;
	if (g_VolumeCB.UseTemporalReprojection)
	{
		historyAlpha = g_VolumeCB.HistoryWeight;
		historyUV = ComputeHistoryVolumeUVW(ComputeWorldSpacePositionFromFroxelIndex(DTid, float3(0.5f, 0.5f, 0.5f)));

		if (any(historyUV < 0.0f) || any(historyUV > 1.0f) || cellWasOccluded)
		{
			historyAlpha = 0.0f;
		}
	}


	float3 in_scattering = emission; // add emission to light scattered into the cameras path at this location

	const float sun_visibility = g_LightCB.DirectionalLight.UseESM ? GetVisibility_ESM(p_ws) : GetVisibility(p_ws); // Sample shadow map to get visibility

	// Evaluate in-scattering from directional light
	{
		const float3 l = -normalize(g_LightCB.DirectionalLight.Direction);
		const float3 el = g_LightCB.DirectionalLight.Intensity * g_LightCB.DirectionalLight.Color;

		const float phase = HGPhaseFunction(v, l, anisotropy);

		in_scattering += phase * sun_visibility * el;
	}

	// Evaluate sky ambient lighting
	{
		in_scattering += sun_visibility * GetSkySHDiffuse(v * anisotropy, g_LightCB.SkyIrradianceEnvironmentMap);
	}


	// Iterate through point lights to evaluate in-scattering
	for (uint i = 0; i < g_LightCB.PointLightCount; i++)
	{
		PointLightGPUData light = g_PointLights[i];

		const float3 l = normalize(light.Position.xyz - p_ws);
		const float3 el = light.Color * light.Intensity;
		const float d = length(light.Position.xyz - p_ws);

		const float phase = HGPhaseFunction(v, l, anisotropy);
		const float atten = distanceAttenuation(d, light.Range);
		in_scattering += phase * el * atten;
	}

	float3 l_scat = in_scattering * scattering;

	if (g_VolumeCB.UseTemporalReprojection)
	{
		float4 historyScatteringAndExtinction = g_PrevLightScatteringVolume.SampleLevel(g_HistoryVolumeSampler, historyUV, 0);

		l_scat = lerp(l_scat, historyScatteringAndExtinction.rgb, historyAlpha);
		extinction = lerp(extinction, historyScatteringAndExtinction.a, historyAlpha);
	}

	g_LightScatteringVolume[DTid] = float4(l_scat, extinction);
}

#endif