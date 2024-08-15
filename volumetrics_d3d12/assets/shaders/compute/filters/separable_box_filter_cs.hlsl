#ifndef SEPARABLEBOXFILTERCS_HLSL
#define SEPARABLEBOXFILTERCS_HLSL


#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"


ConstantBuffer<BoxFilterParamsCB> g_ParamsCB : register(b0);

Texture2D<float4> g_InTexture : register(t0);
RWTexture2D<float4> g_OutTexture : register(u0);


[numthreads(16, 16, 1)]
void HorizontalPass(uint3 DTid : SV_DispatchThreadID)
{
	if (any(DTid.xy >= g_ParamsCB.ResourceDimensions))
		return;

	double4 accum = double4(0.0, 0.0, 0.0, 0.0);
	double weightSum = 0.0;

	const int iMin = DTid.x - (g_ParamsCB.FilterSize / 2);
	const int iMax = DTid.x + (g_ParamsCB.FilterSize / 2);
	for (int i = iMin; i < iMax; i++)
	{
		if (i >= 0 && i < g_ParamsCB.ResourceDimensions.x)
		{
			accum += (double4) (g_InTexture[uint2(i, DTid.y)]);
			weightSum += 1.0;
		}
	}
	g_OutTexture[DTid.xy] = (float4) (accum / weightSum);
}

[numthreads(16, 16, 1)]
void VerticalPass(uint3 DTid : SV_DispatchThreadID)
{
	if (any(DTid.xy >= g_ParamsCB.ResourceDimensions))
		return;
	
	double4 accum = double4(0.0, 0.0, 0.0, 0.0);
	double weightSum = 0.0;

	const int iMin = DTid.y - (g_ParamsCB.FilterSize / 2);
	const int iMax = DTid.y + (g_ParamsCB.FilterSize / 2);
	for (int i = iMin; i < iMax; i++)
	{
		if (i >= 0 && i < g_ParamsCB.ResourceDimensions.y)
		{
			accum += (double4) (g_InTexture[uint2(DTid.x, i)]);
			weightSum += 1.0;
		}
	}
	g_OutTexture[DTid.xy] = (float4) (accum / weightSum);
}

#endif