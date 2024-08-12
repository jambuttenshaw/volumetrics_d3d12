#ifndef VOLUMETRICS_H
#define VOLUMETRICS_H


#define HLSL
#include "../../HlslCompat/HlslCompat.h"


float HGPhaseFunction(float3 v, float3 l, float g)
{
	const float numerator = 1.0f - g * g;
	const float denominator = pow(1.0f + g * g - 2 * g * dot(v, l), 1.5f);
	return numerator / (4.0f * PI * denominator);
}


// Froxel helper functions
float ZSliceToFroxelDepth(float slice, float fogMinZ, float fogMaxZ, uint volumeDepth)
{
	/*
	const float sliceNormalized = (float) (slice) / (float) (volumeDepth - 1);
	return fogMinZ - 1 + pow(1 + fogMaxZ - fogMinZ, sliceNormalized);
	*/
	const float sliceNormalized = (float) (slice) / (float) (volumeDepth - 1);
	return lerp(fogMinZ, fogMaxZ, sliceNormalized);
}

uint FroxelDepthToZSlice(float depth, float fogMinZ, float fogMaxZ, uint volumeDepth)
{
	/*
	const float sliceNormalized = log2(1 + depth - fogMinZ) / log2(1 + fogMaxZ - fogMinZ);
	return saturate(sliceNormalized) * volumeDepth - 1;
	*/
	const float sliceNormalized = (depth - fogMinZ) / (fogMaxZ - fogMinZ);
	return saturate(sliceNormalized) * volumeDepth - 1;
}

// Calculates the W texture coordinate for the volume texture from a given scene depth
float FroxelDepthToVolumeW(float depth, float fogMinZ, float fogMaxZ)
{
	/*
	const float sliceNormalized = log2(1 + depth - fogMinZ) / log2(1 + fogMaxZ - fogMinZ);
	return saturate(sliceNormalized);
	*/
	const float sliceNormalized = (depth - fogMinZ) / (fogMaxZ - fogMinZ);
	return saturate(sliceNormalized);
}


#endif