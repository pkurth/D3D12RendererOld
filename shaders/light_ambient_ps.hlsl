#include "pbr_common.h"

struct ps_input
{
	float2 uv : TEXCOORDS;
};


SamplerState cubemapSampler				: register(s0);
SamplerState brdfSampler				: register(s1);
SamplerState gbufferSampler				: register(s2);

// PBR.
TextureCube<float4> irradianceTexture	: register(t0);
TextureCube<float4> environmentTexture	: register(t1);
Texture2D<float4> brdf					: register(t2);

// GBuffer.
Texture2D<float4> albedos				: register(t3);
Texture2D<float4> normals				: register(t4);


float4 main(ps_input IN) : SV_TARGET
{
	float3 N = normals.Sample(gbufferSampler, IN.uv).xyz;
	float3 V = float3(1,0,0);
	float3 albedo = albedos.Sample(gbufferSampler, IN.uv).rgb;
	float3 ao = float3(1.f, 1.f, 1.f);
	float metalness = 0.f;
	float roughness = 0.8f;

	// Common.
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalness);
	float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.f), F0, roughness);
	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metalness;

	// Diffuse.
	float3 irradiance = irradianceTexture.Sample(cubemapSampler, N).rgb;
	float3 diffuse = irradiance * albedo;

	// Specular.
	float3 R = reflect(-V, N);
	uint width, height, numMipLevels;
	environmentTexture.GetDimensions(0, width, height, numMipLevels);
	float lod = roughness * float(numMipLevels - 1);

	float3 prefilteredColor = environmentTexture.SampleLevel(cubemapSampler, R, lod).rgb;
	float2 envBRDF = brdf.Sample(brdfSampler, float2(roughness, max(dot(N, V), 0.f))).rg;
	float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	float3 ambient = (kD * diffuse + specular) * ao;

	return float4(ambient, 1.f);
}