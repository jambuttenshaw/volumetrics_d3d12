#ifndef VOLUMEINTEGRATIONCS_HLSL
#define VOLUMEINTEGRATIONCS_HLSL

#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

#include "volumetrics.hlsli"


ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
ConstantBuffer<VolumetricsConstantBuffer> g_VolumeCB : register(b1);

RWTexture3D<float4> g_LightScatteringVolume : register(u0);


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float3 accumulatedLighting = float3(0.0f, 0.0f, 0.0f);
	float accumulatedTransmittance = 1.0f;

	float previousSliceDepth = ZSliceToFroxelDepth(0, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance, g_VolumeCB.VolumeResolution.z);

	for (uint layer = 0; layer < g_VolumeCB.VolumeResolution.z; layer++)
	{
		const uint3 layerCoordinate = uint3(DTid.xy, layer);

		float4 scatteringAndExtinction = g_LightScatteringVolume[layerCoordinate];

		const float sliceDepth = ZSliceToFroxelDepth(layer + 0.5f, g_PassCB.NearPlane, g_VolumeCB.MaxVolumeDistance, g_VolumeCB.VolumeResolution.z);
		const float stepLength = sliceDepth - previousSliceDepth;
		previousSliceDepth = sliceDepth;

		const float transmittance = exp(-scatteringAndExtinction.w * stepLength);

		const float3 scatteringIntegratedOverSlice = (scatteringAndExtinction.rgb - scatteringAndExtinction.rgb * transmittance) / max(scatteringAndExtinction.w, 0.00001f);
		accumulatedLighting += scatteringIntegratedOverSlice * accumulatedTransmittance;
		accumulatedTransmittance *= transmittance;

		g_LightScatteringVolume[layerCoordinate] = float4(accumulatedLighting, accumulatedTransmittance);
	}
}

#endif

