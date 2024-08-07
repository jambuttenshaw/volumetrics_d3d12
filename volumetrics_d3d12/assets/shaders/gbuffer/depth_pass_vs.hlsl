#ifndef DEPTHPASSVS_HLSL
#define DEPTHPASSVS_HLSL

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "deferred_rendering.hlsli"

ConstantBuffer<ObjectConstantBuffer> g_ObjectCB : register(b0);
ConstantBuffer<LightingConstantBuffer> g_LightingCB : register(b1);


DepthPass_VSOutput main(/* Same input as geometry pass */ GeometryPass_VSInput input)
{
	DepthPass_VSOutput output;

	const float4 worldPos = mul(input.Position, g_ObjectCB.WorldMat);
	output.Position = mul(worldPos, g_LightingCB.DirectionalLight.ViewProjection);

	return output;
}

#endif