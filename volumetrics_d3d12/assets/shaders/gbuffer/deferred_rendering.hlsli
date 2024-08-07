#ifndef DEFERREDRENDERING_H
#define DEFERREDRENDERING_H


// Structures communicated between shader stages

// General purpose full-screen vertex shader
// That will produce a full-screen quad on the near-plane
struct FullscreenVSOutputType
{
	float4 Position : SV_Position;
	float2 UV : TEXCOORD;
	float3 WSViewDirection : POSITION0; // World-space view from camera through this pixel
};


// Geometry pass pipeline

struct GeometryPass_VSInput
{
	float4 Position : POSITION;
	float3 Normal : NORMAL;
};

struct GeometryPass_VSToPS
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};

struct GeometryPass_PSOutput
{
	float3 Albedo : SV_TARGET0;
	float3 Normal : SV_TARGET1;
	float2 RoughnessMetalness : SV_TARGET2;
};


struct DepthPass_VSOutput // No PS used for depth pass
{
	float4 Position : SV_POSITION;
};

#endif
