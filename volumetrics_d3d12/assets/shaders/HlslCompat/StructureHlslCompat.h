#ifndef STRUCTUREHLSLCOMPAT_H
#define STRUCTUREHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


struct Ray
{
	XMFLOAT3 origin;
	XMFLOAT3 direction;
};

struct AABB
{
	XMFLOAT3 TopLeft;
	XMFLOAT3 BottomRight;
};


// Constant attributes per frame
struct PassConstantBuffer
{
	XMMATRIX View;
	XMMATRIX InvView;
	XMMATRIX Proj;
	XMMATRIX InvProj;
	XMMATRIX ViewProj;
	XMMATRIX InvViewProj;

	XMFLOAT3 WorldEyePos;

	UINT Flags;

	float TotalTime;
	float DeltaTime;

	// The number of lights in the light buffer
	UINT LightCount;

	UINT Padding;
};

// Per-object data per frame
struct ObjectConstantBuffer
{
	XMMATRIX WorldMat;
	UINT MaterialID;
};


// Properties used for full-screen quad raster passes
struct FullscreenQuadProperties
{
	// Which depth (0-1) should the fullscreen quad be placed at
	float Depth;
};


// Lighting structures
struct LightGPUData
{
	XMFLOAT4 Position; // Directional light if w == 0

	XMFLOAT3 Color;
	float Intensity;
};

struct MaterialGPUData
{
	XMFLOAT3 Albedo;
	float Roughness;
	float Metalness;
};


#endif