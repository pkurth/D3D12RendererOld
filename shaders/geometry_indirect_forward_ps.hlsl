#include "normals.hlsli"
#include "material.hlsli"
#include "pbr.hlsli"
#include "camera.hlsli"
#include "light_probe.hlsli"
#include "lighting.hlsli"

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;
	nointerpolation uint lightProbeTetrahedron : LIGHTPROBE_TETRAHEDRON;
};

struct ps_output
{
	float4 color	: SV_Target0;
};

ConstantBuffer<camera_cb> camera			: register(b0);
ConstantBuffer<material_cb> material		: register(b2);
ConstantBuffer<directional_light> sun		: register(b3);
ConstantBuffer<spot_light> spotLight		: register(b4);


SamplerState linearWrapSampler				: register(s0, space0);
SamplerState brdfSampler					: register(s1, space0);
SamplerComparisonState shadowMapSampler		: register(s2, space0);



// BRDF.
TextureCube<float4> irradianceTexture		: register(t0, space0);
TextureCube<float4> environmentTexture		: register(t1, space0);
Texture2D<float4> brdf						: register(t2, space0);

// Materials.
Texture2D<float4> albedoTextures[]	: register(t0, space1);
Texture2D<float4> normalTextures[]	: register(t0, space2);
Texture2D<float> roughnessTextures[]	: register(t0, space3);
Texture2D<float> metallicTextures[]	: register(t0, space4);

// Shadow maps.
Texture2D<float> sunShadowMapCascades[4]	: register(t0, space5);
Texture2D<float> spotLightShadowMap			: register(t4, space5);

// Light probes.
StructuredBuffer<float4> lightProbePositions					: register(t0, space6);
StructuredBuffer<packed_spherical_harmonics> sphericalHarmonics	: register(t1, space6);
StructuredBuffer<light_probe_tetrahedron> lightProbeTetrahedra	: register(t2, space6);


ps_output main(ps_input IN)
{
	uint textureID = material.textureID_usageFlags >> 16;
	uint usageFlags = material.textureID_usageFlags & 0xFFFF;

	float4 albedo = ((usageFlags & USE_ALBEDO_TEXTURE)
		? albedoTextures[textureID].Sample(linearWrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	float3 N = (usageFlags & USE_NORMAL_TEXTURE)
		? mul(normalTextures[textureID].Sample(linearWrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f), IN.tbn)
		: IN.tbn[2];

	float roughness = (usageFlags & USE_ROUGHNESS_TEXTURE)
		? roughnessTextures[textureID].Sample(linearWrapSampler, IN.uv)
		: material.roughnessOverride;
	roughness = clamp(roughness, 0.01f, 0.99f);

	float metallic = (usageFlags & USE_METALLIC_TEXTURE)
		? metallicTextures[textureID].Sample(linearWrapSampler, IN.uv)
		: material.metallicOverride;
	float ao = 1.f;// (material.usageFlags & USE_AO_TEXTURE) ? RMAO.z : 1.f;


	float3 camToP = IN.worldPosition - camera.position.xyz;
	float3 V = -normalize(camToP);
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.xyz, metallic);

	float4 totalLighting = float4(0.f, 0.f, 0.f, albedo.w);


	// Ambient.
#if 1
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz, irradianceTexture, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
#else
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz,
		lightProbePositions, lightProbeTetrahedra, IN.worldPosition, IN.lightProbeTetrahedron, sphericalHarmonics,
		environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
#endif

#if 1
	// Sun.
	{
		uint numCascades = sun.numShadowCascades;
		float4 cascadeDistances = sun.cascadeDistances;

		float currentPixelDepth = dot(camera.forward.xyz, camToP);
		float4 comparison = float4(currentPixelDepth, currentPixelDepth, currentPixelDepth, currentPixelDepth) > cascadeDistances;
		float index = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);

		index = min(index, float(numCascades - 1));

		int currentCascadeIndex = int(index);
		int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

		float4 bias = sun.bias;
		float visibility = sampleShadowMap(sun.vp[currentCascadeIndex], IN.worldPosition, sunShadowMapCascades[currentCascadeIndex],
			shadowMapSampler, sun.texelSize, bias[currentCascadeIndex]);

		// Blend between cascades.
		float currentPixelsBlendBandLocation = 1.f;
		if (numCascades > 1)
		{
			// Calculate blend amount.
			int blendIntervalBelowIndex = max(0, currentCascadeIndex - 1);
			float cascade0Factor = float(currentCascadeIndex > 0);
			float pixelDepth = currentPixelDepth - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;
			float blendInterval = cascadeDistances[currentCascadeIndex] - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;

			// Relative to current cascade. 0 means at nearplane of cascade, 1 at farplane of cascade.
			currentPixelsBlendBandLocation = 1.f - pixelDepth / blendInterval;
		}
		if (currentPixelsBlendBandLocation < sun.blendArea) // Blend area is relative!
		{
			float blendBetweenCascadesAmount = currentPixelsBlendBandLocation / sun.blendArea;
			float visibilityOfNextCascade = sampleShadowMap(sun.vp[nextCascadeIndex], IN.worldPosition, sunShadowMapCascades[nextCascadeIndex], 
				shadowMapSampler, sun.texelSize, bias[nextCascadeIndex]);
			visibility = lerp(visibilityOfNextCascade, visibility, blendBetweenCascadesAmount);
		}
		
		float3 L = -sun.worldSpaceDirection.xyz;
		float3 radiance = sun.color.xyz * visibility; // No attenuation for sun.

		totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
	}
#endif

#if 1
	{
		float distance = length(spotLight.worldSpacePosition.xyz - IN.worldPosition);
		float3 L = (spotLight.worldSpacePosition.xyz - IN.worldPosition) / distance;

		float theta = dot(-L, spotLight.worldSpaceDirection.xyz);
		if (theta > spotLight.outerCutoff)
		{
			float attenuation = getAttenuation(spotLight.attenuation, distance);

			float epsilon = spotLight.innerCutoff - spotLight.outerCutoff;
			float intensity = saturate((theta - spotLight.outerCutoff) / epsilon);

			float visibility = sampleShadowMap(spotLight.vp, IN.worldPosition, spotLightShadowMap, shadowMapSampler, spotLight.texelSize, spotLight.bias);

			float totalIntensity = intensity * attenuation * visibility;
			if (totalIntensity > 0.f)
			{
				float3 radiance = spotLight.color.xyz * totalIntensity;
				totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
			}
		}
	}
#endif

	ps_output OUT;
	OUT.color = totalLighting;
	return OUT;
}