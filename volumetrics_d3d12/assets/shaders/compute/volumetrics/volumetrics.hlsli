#ifndef VOLUMETRICS_H
#define VOLUMETRICS_H


#define HLSL
#include "../../HlslCompat/HlslCompat.h"


float HGPhaseFunction(float3 v, float3 l, float g)
{
	const float numerator = 1.0f - g * g;
	const float denominator = pow((1.0f + g * g - 2 * g * dot(v, l)), 1.5f);
	return numerator / (4.0f * PI * denominator);
}


#endif