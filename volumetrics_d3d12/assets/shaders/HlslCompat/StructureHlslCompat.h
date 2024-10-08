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

	// For temporal effects that use reprojection
	XMMATRIX PrevView;
	XMMATRIX PrevProj;
	XMMATRIX PrevViewProj;

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

	UINT FrameIndexMod16;
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
struct LightingConstantBuffer
{
	// Global directional light (the sun)
	struct
	{
		XMFLOAT3 Direction;
		float Intensity;
		XMFLOAT3 Color;
		UINT UseESM;

		XMMATRIX ViewProjection;
	} DirectionalLight;

	// From "Stupid Spherical Harmonics (SH) Tricks"
	XMFLOAT4 SkyIrradianceEnvironmentMap[7];
	UINT UseSHIrradiance;

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

#define VOLUME_FLAGS_DISABLE_AMBIENT		(1 << 0)
#define VOLUME_FLAGS_DISABLE_SUN			(1 << 1)
#define VOLUME_FLAGS_DISABLE_POINT_LIGHTS	(1 << 2)


struct VolumetricsConstantBuffer
{
	// Volume properties
	XMUINT3 VolumeResolution;
	float MaxVolumeDistance;	// The furthest view-space depth that the volume maps to

	// Temporal reprojection
	UINT UseTemporalReprojection;
	float LightScatteringJitterMultiplier;

	float HistoryWeight;

	UINT Flags;
};

// Collection of parameters required to calculate light scattering
struct LightScatteringConstantBuffer
{
	// Depth buffer dimensions (For testing froxel occlusion)
	XMUINT2 DepthBufferDimensions;
	XMUINT2 PrevDepthBufferDimensions;
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
	XMUINT2 InDimensions;
	XMUINT2 OutDimensions;
};


struct BoxFilterParamsCB
{
	XMUINT2 ResourceDimensions;
	UINT FilterSize;
};

#endif