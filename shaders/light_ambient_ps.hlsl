#include "pbr.h"
#include "normals.h"

struct ps_input
{
	float2 uv : TEXCOORDS;
	float3 V  : VIEWDIR;
};


SamplerState linearClampSampler			: register(s0);	

// PBR.
TextureCube<float4> irradianceTexture	: register(t0);
TextureCube<float4> environmentTexture	: register(t1);
Texture2D<float4> brdf					: register(t2);

// GBuffer.
Texture2D<float4> albedos				: register(t3);
Texture2D<float4> normalsRoughMetal		: register(t4);


float4 main(ps_input IN) : SV_TARGET
{
	float4 NRM = normalsRoughMetal.Sample(linearClampSampler, IN.uv);
	float3 N = decodeNormal(NRM.xy);
	float3 V = normalize(-IN.V);
	float4 albedoAO = albedos.Sample(linearClampSampler, IN.uv);
	float3 albedo = albedoAO.rgb;
	float ao = albedoAO.w;
	float roughness = clamp(NRM.z, 0.01f, 0.99f);
	float metalness = NRM.w;

	// Common.
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalness);
	float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.f), F0, roughness);
	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metalness;

	// Diffuse.
	float3 irradiance = irradianceTexture.Sample(linearClampSampler, N).rgb;
	float3 diffuse = irradiance * albedo;

	// Specular.
	float3 R = reflect(-V, N);
	uint width, height, numMipLevels;
	environmentTexture.GetDimensions(0, width, height, numMipLevels);
	float lod = roughness * float(numMipLevels - 1);

	float3 prefilteredColor = environmentTexture.SampleLevel(linearClampSampler, R, lod).rgb;
	float2 envBRDF = brdf.Sample(linearClampSampler, float2(roughness, max(dot(N, V), 0.f))).rg;
	float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	float3 ambient = (kD * diffuse + specular) * ao;

	return float4(ambient, 1.f);
}