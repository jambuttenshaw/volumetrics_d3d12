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

	XMFLOAT2 Padding;
};

// Per-object data per frame
struct ObjectConstantBuffer
{
	XMMATRIX WorldMat;
};


// Lighting structures
struct LightGPUData
{
	XMFLOAT3 Direction;

	float Intensity;
	XMFLOAT3 Color;

	float Padding;

};

struct MaterialGPUData
{
	XMFLOAT3 Albedo;
	float Roughness;
	float Metalness;
	float Reflectance;
};


#endif