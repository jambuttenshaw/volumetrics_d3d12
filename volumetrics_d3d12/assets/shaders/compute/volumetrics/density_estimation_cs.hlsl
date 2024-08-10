#ifndef DENSITYESTIMATIONCS_HLSL
#define DENSITYESTIMATIONCS_HLSL

#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

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


[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	g_VBufferA[DTid] = float4(0.5f, 0.5f, 0.5f, 0.5f);
	g_VBufferB[DTid] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

#endif