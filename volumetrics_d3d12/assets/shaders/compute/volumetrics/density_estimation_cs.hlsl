#ifndef DENSITYESTIMATIONCS_HLSL
#define DENSITYESTIMATIONCS_HLSL

#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

#include "volumetrics.hlsli"
#include "../../include/simplex_noise.hlsli"


ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
ConstantBuffer<VolumetricsConstantBuffer> g_VolumeCB : register(b1);
ConstantBuffer<GlobalFogConstantBuffer> g_GlobalFogCB : register(b2);

/*
 * Format: RGBA16F
 * RGB - Scattering
 * A   - Extinction
*/
RWTexture3D<float4> g_VBufferA : register(u0);
/*
 * Format: RGBA16F
 * RGB - Emissive
 * A   - Phase (g)
*/
RWTexture3D<float4> g_VBufferB : register(u1);


float3 ComputeWorldSpacePositionFromFroxelIndex(uint3 froxel)
{
	const float depth = ZSliceToFroxelDepth(froxel.z, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance, g_VolumeCB.VolumeResolution.z);

	// Need to get the depth in NDC
	const float depthNDC = g_PassCB.ViewDepthToNDC.x - g_PassCB.ViewDepthToNDC.y / depth;

	// we want to calculate at the center of each froxel
	const float2 sliceUV = (froxel.xy + float2(0.5f, 0.5f)) / (float2) (g_VolumeCB.VolumeResolution.xy);
	float2 sliceNDC = (2.0f * sliceUV - 1.0f) * float2(1.0f, -1.0f);

	float4 p_vs = mul(float4(sliceNDC, depthNDC, 1.0f), g_PassCB.InvProj);
	p_vs /= p_vs.w;
	return mul(p_vs, g_PassCB.InvView).xyz;
}


[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float extinction = g_GlobalFogCB.Extinction;

	float3 emission = g_GlobalFogCB.Emission;
	float anisotropy = g_GlobalFogCB.Anisotropy;

	// Calculate fall-offs as a function of world space position

	// Get world-space pos
	const float3 p_ws = ComputeWorldSpacePositionFromFroxelIndex(DTid);

	// Compute a noise value from the world space position
	/*
	float noise = 0.0f;
	float freq = 0.05f;
	float amp = 0.005f;
	for (uint octave = 0; octave < 4; octave++)
	{
		noise += amp * snoise(p_ws * freq + (0.1f * g_PassCB.TotalTime));
		amp *= 0.5f;
		freq *= 2.0f;
	}
	noise *= 0.5f;
	extinction *= noise;
	*/

	// Confine fog to a small region
	const float r = length(p_ws.xz);
	extinction *= 1.0f - smoothstep(5.0f, 8.0f, r);
	extinction *= smoothstep(0.0f, 1.0f, p_ws.y) * (1.0f - smoothstep(3.0f, 4.0f, p_ws.y));
	
	float3 scattering = g_GlobalFogCB.Albedo * extinction;
	g_VBufferA[DTid] = float4(scattering, extinction);
	g_VBufferB[DTid] = float4(emission, anisotropy);
}

#endif