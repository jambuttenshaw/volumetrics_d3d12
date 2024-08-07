#ifndef GEOMETRYPASSVS_HLSL
#define GEOMETRYPASSVS_HLSL

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "deferred_rendering.hlsli"

ConstantBuffer<ObjectConstantBuffer> g_ObjectCB : register(b0);
ConstantBuffer<PassConstantBuffer> g_PassCB : register(b1);


GeometryPass_VSToPS main(GeometryPass_VSInput input)
{
	GeometryPass_VSToPS output;

	const float4 worldPos = mul(input.Position, g_ObjectCB.WorldMat);
	output.Position = mul(worldPos, g_PassCB.ViewProj);

	output.Normal = mul(float4(input.Normal, 0.0f), g_ObjectCB.WorldMat).xyz;

	return output;
}

#endif