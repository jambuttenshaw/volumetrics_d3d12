#ifndef GEOMETRYPASSPS_HLSL
#define GEOMETRYPASSPS_HLSL

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "deferred_rendering.hlsli"

ConstantBuffer<ObjectConstantBuffer> g_ObjectCB : register(b0);
ConstantBuffer<PassConstantBuffer> g_PassCB : register(b1);

StructuredBuffer<MaterialGPUData> g_MaterialBuffer : register(t0);


GeometryPass_PSOutput main(GeometryPass_VSToPS input)
{
	GeometryPass_PSOutput output;

	output.Normal = 0.5f * normalize(input.Normal) + 0.5f;

	// Load material parameters
	const MaterialGPUData material = g_MaterialBuffer[g_ObjectCB.MaterialID];

	output.Albedo = material.Albedo;
	output.RoughnessMetalness = float2(material.Roughness, material.Metalness);

	return output;
}


#endif