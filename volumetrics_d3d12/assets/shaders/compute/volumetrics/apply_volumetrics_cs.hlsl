#ifndef APPLYVOLUMETRICSCS_HLSL
#define APPLYVOLUMETRICSCS_HLSL


#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

#include "volumetrics.hlsli"


ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
ConstantBuffer<VolumetricsConstantBuffer> g_VolumeCB : register(b1);


Texture3D<float4> g_LightScatteringVolume : register(t0);

Texture2D<float> g_DepthBuffer : register(t1);


SamplerState g_LightScatteringSampler : register(s0);


RWTexture2D<float4> g_Output : register(u0);


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (any(DTid.xy >= g_PassCB.RTSize))
		return;

	const float3 pixelWithoutFog = g_Output[DTid.xy].rgb;
	const float sceneDepth = g_DepthBuffer[DTid.xy].r;

	const float2 uv = (DTid.xy + float2(0.5f, 0.5f)) * g_PassCB.InvRTSize;

	// get view position
	float4 p_ndc = float4(2.0f * uv - 1.0f, sceneDepth, 1.0f);
	p_ndc.y = -p_ndc.y;

	float4 p_vs = mul(p_ndc, g_PassCB.InvProj);
	p_vs /= p_vs.w;

	float volumeZ = FroxelDepthToVolumeW(p_vs.z, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance);

	const float3 positionInVolume = float3(uv, volumeZ);

	float4 scatteringAndTransmittance = g_LightScatteringVolume.SampleLevel(g_LightScatteringSampler, positionInVolume, 0);
	g_Output[DTid.xy] = float4(pixelWithoutFog * scatteringAndTransmittance.www + scatteringAndTransmittance.xyz, 1.0f);
}

#endif