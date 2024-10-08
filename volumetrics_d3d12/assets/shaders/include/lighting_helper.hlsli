#ifndef LIGHTINGHELPER_H
#define LIGHTINGHELPER_H


// DIFFUSE NDF's
float3 lambertian_diffuse(float3 albedo)
{
	return albedo / PI;
}


// SPECULAR NDF's
float ggx_ndf(float3 n, float3 h, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(n, h), 0.0);
	float NdotH2 = NdotH * NdotH;
	
	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return num / denom;
}


// FRESNEL
float3 shlick_fresnel_reflectance(float3 f0, float3 v, float3 h)
{
    // shlick approximation of the fresnel equations
	return f0 + (1.0f - f0) * pow(1.0f - saturate(dot(h, v)), 5.0f);
}


// GEOMETRY/VISIBILITY FUNCTIONS
float schlickggx_geometry(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	
	return num / denom;
}

float smith_geometry(float3 n, float3 v, float3 l, float roughness)
{
	float NdotV = saturate(dot(n, v));
	float NdotL = saturate(dot(n, l));
	float ggx1 = schlickggx_geometry(NdotV, roughness);
	float ggx2 = schlickggx_geometry(NdotL, roughness);
	
	return ggx1 * ggx2;
}


// BRDF
float3 ggx_brdf(float3 v, float3 l, float3 n, float3 albedo, float3 f0, float roughness, float metallic)
{
	float3 h = normalize(l + v);
 
	// specular
	
	// fresnel effect tells us how much light is reflected
	float3 Rf = shlick_fresnel_reflectance(f0, v, h);
	float ndf = ggx_ndf(n, h, roughness);
	float G = smith_geometry(n, v, l, roughness);

	float3 numerator = Rf * ndf * G;
	float denominator = 4.0f * saturate(dot(n, v)) * saturate(dot(n, l)) + 0.0001f;
	
	float3 fspec = numerator / denominator;

	// diffuse
	
	// all light that is not reflected is diffused
	float3 cdiff = albedo - f0;
	cdiff *= 1.0f - metallic;
	
	float3 fdiff = lambertian_diffuse(cdiff);
	
	return fdiff + fspec;
}


// Attenuation
float distanceAttenuation(float r, float rmax)
{
	const float a1 = r / max(rmax, 0.0001f);
	const float a2 = max(0.0f, 1.0f - (a1 * a1));
	return a2 * a2;
}


/** 
 * Computes sky diffuse lighting from the SH irradiance map.  
 * This has the SH basis evaluation and diffuse convolution weights combined for minimal ALU's - see "Stupid Spherical Harmonics (SH) Tricks" 
 */
float3 GetSkySHDiffuse(float3 Normal, float4 SkyIrradianceEnvironmentMap[7])
{
	float4 NormalVector = float4(Normal, 1.0f);

	float3 Intermediate0, Intermediate1, Intermediate2;
	Intermediate0.x = dot(SkyIrradianceEnvironmentMap[0], NormalVector);
	Intermediate0.y = dot(SkyIrradianceEnvironmentMap[1], NormalVector);
	Intermediate0.z = dot(SkyIrradianceEnvironmentMap[2], NormalVector);

	float4 vB = NormalVector.xyzz * NormalVector.yzzx;
	Intermediate1.x = dot(SkyIrradianceEnvironmentMap[3], vB);
	Intermediate1.y = dot(SkyIrradianceEnvironmentMap[4], vB);
	Intermediate1.z = dot(SkyIrradianceEnvironmentMap[5], vB);

	float vC = NormalVector.x * NormalVector.x - NormalVector.y * NormalVector.y;
	Intermediate2 = SkyIrradianceEnvironmentMap[6].xyz * vC;

	// max to not get negative colors
	return max(0, Intermediate0 + Intermediate1 + Intermediate2);
}

#endif