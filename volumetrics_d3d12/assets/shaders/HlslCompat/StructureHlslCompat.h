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

	XMUINT2 RTSize;
	XMFLOAT2 InvRTSize;

	float NearPlane;
	float FarPlane;

	// Coefficients required to transform depth from view-space depth to NDC
	XMFLOAT2 ViewDepthToNDC;

	float TotalTime;
	float DeltaTime;

	XMFLOAT2 Padding;
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
struct LightingConstantBuffer
{
	// Global directional light (the sun)
	struct
	{
		XMFLOAT3 Direction;
		float Intensity;
		XMFLOAT3 Color;
		float Padding;

		XMMATRIX ViewProjection;
	} DirectionalLight;

	UINT PointLightCount;
};

struct PointLightGPUData
{
	XMFLOAT3 Position;
	float Range;

	XMFLOAT3 Color;
	float Intensity;
};

struct MaterialGPUData
{
	XMFLOAT3 Albedo;
	float Roughness;
	float Metalness;
};



// Volumetrics

struct VolumetricsConstantBuffer
{
	XMUINT3 VolumeResolution;
	float MaxVolumeDistance;	// The furthest view-space depth that the volume maps to
};

struct GlobalFogConstantBuffer
{
	XMFLOAT3 Albedo;
	float Extinction;

	XMFLOAT3 Emission;
	float Anisotropy;

	// Fog geometry
	float MaxHeight;
	float HeightSmoothing;

	float Radius;
	float RadiusSmoothing;
};


// Post-processing

struct TonemappingParametersConstantBuffer
{
	XMUINT2 OutputDimensions;
};


// ESM
#define ESM_EXPONENT 80

struct ExponentialShadowMapCB
{
	XMUINT2 OutDimensions;
};

#endif