#ifndef BASICPS_HLSL
#define BASICPS_HLSL

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "deferred_rendering.hlsli"

ConstantBuffer<ObjectConstantBuffer> g_ObjectCB : register(b0);
ConstantBuffer<PassConstantBuffer> g_PassCB : register(b1);


GeometryPass_PSOutput main(GeometryPass_VSToPS input)
{
	GeometryPass_PSOutput output;

	output.Albedo = float3(1.0f, 0.2f, 0.2f);
	output.Normal = 0.5f * normalize(input.Normal) + 0.5f;
	output.RoughnessMetalness = float2(0.1f, 0.0f);

	return output;
}


#endif