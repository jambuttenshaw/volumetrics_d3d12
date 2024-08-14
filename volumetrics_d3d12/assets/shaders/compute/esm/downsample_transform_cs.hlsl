#ifndef DOWNSAMPLETRANSFORMCS_HLSL
#define DOWNSAMPLETRANSFORMCS_HLSL


#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"


ConstantBuffer<ExponentialShadowMapCB> g_ParamsCB : register(b0);

Texture2D g_InShadowMap : register(t0);
RWTexture2D<float> g_OutExponentialShadowMap : register(u0);

SamplerState g_PointSampler : register(s0);


[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadId)
{
	if (any(DTid.xy >= g_ParamsCB.OutDimensions))
		return;

	float2 uv = (DTid.xy + float2(0.5f, 0.5f)) / (float2) (g_ParamsCB.OutDimensions);

	float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);
	accum += exp(g_InShadowMap.GatherRed(g_PointSampler, uv, int2(0, 0)) * ESM_EXPONENT);
	accum += exp(g_InShadowMap.GatherRed(g_PointSampler, uv, int2(2, 0)) * ESM_EXPONENT);
	accum += exp(g_InShadowMap.GatherRed(g_PointSampler, uv, int2(0, 2)) * ESM_EXPONENT);
	accum += exp(g_InShadowMap.GatherRed(g_PointSampler, uv, int2(2, 2)) * ESM_EXPONENT);

	g_OutExponentialShadowMap[DTid.xy] = dot(accum, 1.0f / 16.0f);
}

#endif