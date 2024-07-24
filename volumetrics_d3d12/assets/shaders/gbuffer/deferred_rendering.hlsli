#ifndef DEFERREDRENDERING_H
#define DEFERREDRENDERING_H


// Structures communicated between shader stages
struct VSInput
{
	float4 Position : POSITION;
	float3 Normal : NORMAL;
};

struct VSToPS
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};

struct PSOutput
{
	float3 Albedo : SV_TARGET0;
	float3 Normal : SV_TARGET1;
	float2 RoughnessMetalness : SV_TARGET2;
};

#endif
