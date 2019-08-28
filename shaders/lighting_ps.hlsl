#include "pbr.h"
#include "normals.h"
#include "camera.h"

ConstantBuffer<camera_cb> camera : register(b0);

struct ps_input
{
	float2 uv : TEXCOORDS;
	float3 V  : VIEWDIR;
};


SamplerState gbufferSampler					: register(s0);
SamplerState brdfSampler					: register(s1);
SamplerComparisonState shadowMapSampler		: register(s2);

// PBR.
TextureCube<float4> irradianceTexture		: register(t0);
TextureCube<float4> environmentTexture		: register(t1);
Texture2D<float4> brdf						: register(t2);

// GBuffer.
Texture2D<float4> albedos					: register(t3);
Texture2D<float4> normalsRoughMetal			: register(t4);
Texture2D<float> depthBuffer				: register(t5);

// Shadow maps.
Texture2D<float> sunShadowMapCascades[4]	: register(t6);


ConstantBuffer<directional_light> sunLight : register(b1);


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

	return (kD * albedo * oneOverPI + specular) * radiance * NdotL;
}

float4 main(ps_input IN) : SV_TARGET
{
	float depthBufferDepth = depthBuffer.Sample(gbufferSampler, IN.uv);
	float worldDepth = depthBufferDepthToLinearWorldDepthEyeToFarPlane(depthBufferDepth, camera.projectionParams);

	float3 worldPosition = IN.V * worldDepth + camera.position.xyz;

	float4 NRM = normalsRoughMetal.Sample(gbufferSampler, IN.uv);
	float3 N = decodeNormal(NRM.xy);
	float3 V = normalize(-IN.V);
	float4 albedoAO = albedos.Sample(gbufferSampler, IN.uv);
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
		float3 irradiance = irradianceTexture.Sample(brdfSampler, N).rgb;
		float3 diffuse = irradiance * albedo;

		// Specular.
		float3 R = reflect(-V, N);
		uint width, height, numMipLevels;
		environmentTexture.GetDimensions(0, width, height, numMipLevels);
		float lod = roughness * float(numMipLevels - 1);

		float3 prefilteredColor = environmentTexture.SampleLevel(brdfSampler, R, lod).rgb;
		float2 envBRDF = brdf.Sample(brdfSampler, float2(roughness, NdotV)).rg;
		float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

		float3 ambient = (kD * diffuse + specular) * ao;

		totalLighting.xyz += ambient;
	}

	// Sun.
	{
		uint numCascades = sunLight.numShadowCascades;
		float4 cascadeRelativeDistances = pow(float4(1.f, 2.f, 3.f, 4.f) / numCascades, 2.f);
		float nearPlane = camera.projectionParams.x;
		float farPlane = camera.projectionParams.y;
		float4 cascadeDistances = float4(
			lerp(nearPlane, farPlane, cascadeRelativeDistances.x),
			lerp(nearPlane, farPlane, cascadeRelativeDistances.y),
			lerp(nearPlane, farPlane, cascadeRelativeDistances.z),
			lerp(nearPlane, farPlane, cascadeRelativeDistances.w)
		);

		float4 comparison = float4(worldDepth, worldDepth, worldDepth, worldDepth) > cascadeDistances;
		float index = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);

		index = min(index, float(numCascades) - 1.f);
		uint currentCascadeIndex = uint(index);
		uint nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

		float currentPixelsBlendBandLocation = 1.f;
		//if (numCascades > 1)
		//{
		//	// Calculate blend amount.
		//	int blendIntervalBelowIndex = max(0, currentCascadeIndex - 1);
		//	float cascade0Factor = float(currentCascadeIndex > 0); // if cascade == 0, currentPixelDepth is already relative to current cascade
		//	float pixelDepth = worldDepth - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;
		//	float blendInterval = cascadeDistances[currentCascadeIndex] - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;

		//	// relative to current cascade. 0 means at nearplane of cascade, 1 at farplane of cascade
		//	currentPixelsBlendBandLocation = 1.f - pixelDepth / blendInterval;
		//}



		float3 L = -sunLight.worldSpaceDirection.xyz;
		float3 radiance = sunLight.color.xyz; // No attenuation for sun.

		float4 lightProjected = mul(sunLight.vp[currentCascadeIndex], float4(worldPosition, 1.f));
		// Since the sun is a directional (orthographic) light source, we don't need to divide by w.

		float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
		lightUV.y = 1.f - lightUV.y;

		float shadowValue = 0.f;
		uint count = 0;
		
		float texelSize = 1.f / 2048.f;
		for (int y = -2; y <= 2; ++y)
		{
			for (int x = -2; x <= 2; ++x)
			{
				shadowValue += sunShadowMapCascades[currentCascadeIndex].SampleCmpLevelZero(shadowMapSampler, lightUV + float2(x, y) * texelSize, lightProjected.z - 0.001f);
				++count;
			}
		}
		shadowValue /= count;

		totalLighting.xyz += calculateLighting(albedo, radiance, N, L, V, F0, roughness, metallic) * shadowValue;
	}

	return totalLighting;
}