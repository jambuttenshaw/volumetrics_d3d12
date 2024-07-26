#ifndef SKYBOXPS_HLSL
#define SKYBOXPS_HLSL


#define HLSL
#include "deferred_rendering.hlsli"


// Environment map
TextureCube g_EnvironmentMap : register(t0);

SamplerState g_EnvironmentMapSampler : register(s0);


float4 main(FullscreenVSOutputType input) : SV_TARGET0
{
	return g_EnvironmentMap.Sample(g_EnvironmentMapSampler, normalize(input.WSViewDirection));
}


#endif