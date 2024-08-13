#ifndef TONEMAPPINGCS_HLSL
#define TONEMAPPINGCS_HLSL

#define HLSL
#include "../../HlslCompat/StructureHlslCompat.h"

ConstantBuffer<TonemappingParametersConstantBuffer> g_TonemappingParametersCB : register(b0);

RWTexture2D<float4> g_OutputResource : register(u0);


[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (any(DTid.xy >= g_TonemappingParametersCB.OutputDimensions))
	{
		return;
	}

	float3 color = g_OutputResource[DTid.xy].rgb;

	// Tone mapping
	color = color / (1.0f + color);
	// Gamma correction
	color = pow(color, 0.4545f);

	g_OutputResource[DTid.xy] = float4(color, 1.0f);
}

#endif