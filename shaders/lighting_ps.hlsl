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


ConstantBuffer<directional_light> directionalLight : register(b1);


static float3 calculateLighting(float3 albedo, float3 radiance, float3 N, float3 L, float3 V, float3 F0, float roughness, float metallic)
{
	float3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 0.f);

	// Cook-Torrance BRDF.
	float NDF = distributionGGX(N, H, roughness);
	float G = geometrySmith(N, V, L, roughness);
	float3 F = fresnelSchlick(max(dot(H, V), 0.f), F0);

	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metallic;

	float NdotL = max(dot(N, L), 0.f);
	float3 numerator = NDF * G * F;
	float denominator = 4.f * NdotV * NdotL;
	float3 specular = numerator / max(denominator, 0.001f);

	return (kD * albedo / pi + specular) * radiance * NdotL;
}

float4 main(ps_input IN) : SV_TARGET
{
	float4 NRM = normalsRoughMetal.Sample(linearClampSampler, IN.uv);
	float3 N = decodeNormal(NRM.xy);
	float3 V = normalize(-IN.V);
	float4 albedoAO = albedos.Sample(linearClampSampler, IN.uv);
	float3 albedo = albedoAO.rgb;
	float ao = albedoAO.w;
	float roughness = clamp(NRM.z, 0.01f, 0.99f);
	float metallic = NRM.w;

	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

	float4 totalLighting = float4(0.f, 0.f, 0.f, 1.f);

	// Ambient.
	{
		// Common.
		float NdotV = max(dot(N, V), 0.f);
		float3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
		float3 kS = F;
		float3 kD = float3(1.f, 1.f, 1.f) - kS;
		kD *= 1.f - metallic;

		// Diffuse.
		float3 irradiance = irradianceTexture.Sample(linearClampSampler, N).rgb;
		float3 diffuse = irradiance * albedo;

		// Specular.
		float3 R = reflect(-V, N);
		uint width, height, numMipLevels;
		environmentTexture.GetDimensions(0, width, height, numMipLevels);
		float lod = roughness * float(numMipLevels - 1);

		float3 prefilteredColor = environmentTexture.SampleLevel(linearClampSampler, R, lod).rgb;
		float2 envBRDF = brdf.Sample(linearClampSampler, float2(roughness, NdotV)).rg;
		float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

		float3 ambient = (kD * diffuse + specular) * ao;

		totalLighting.xyz += ambient;
	}

	// Directional.
	{
		float3 L = -directionalLight.worldSpaceDirection.xyz;
		float3 radiance = directionalLight.color.xyz;

		totalLighting.xyz += calculateLighting(albedo, radiance, N, L, V, F0, roughness, metallic);
	}

	return totalLighting;
}