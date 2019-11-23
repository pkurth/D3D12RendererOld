#include "normals.h"
#include "material.h"
#include "pbr.h"
#include "camera.h"


struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;
};

struct ps_output
{
	float4 color	: SV_Target0;
};

ConstantBuffer<camera_cb> camera			: register(b0);
ConstantBuffer<material_cb> material		: register(b2);
ConstantBuffer<directional_light> sunLight	: register(b3);


SamplerState linearWrapSampler				: register(s0, space0);
SamplerState brdfSampler					: register(s1, space0);
SamplerComparisonState shadowMapSampler		: register(s2, space0);



// PBR.
TextureCube<float4> irradianceTexture		: register(t0, space0);
TextureCube<float4> environmentTexture		: register(t1, space0);
Texture2D<float4> brdf						: register(t2, space0);

// Materials.
Texture2D<float4> albedoTextures[64]	: register(t0, space1);
Texture2D<float4> normalTextures[64]	: register(t0, space2);
Texture2D<float> roughnessTextures[64]	: register(t0, space3);
Texture2D<float> metallicTextures[64]	: register(t0, space4);

// Shadow maps.
Texture2D<float> sunShadowMapCascades[4]	: register(t0, space5);
StructuredBuffer<point_light> pointLights	: register(t4, space5);
StructuredBuffer<spherical_harmonics> shs	: register(t5, space5);


static const spherical_harmonics sh = {
	float4(0.445560, 0.264096, 0.266335, 0.0),
	float4(-0.311523, -0.084572, 0.080637, 0.0),
	float4(0.086516, 0.022305, -0.045806, 0.0),
	float4(-0.091761, 0.026771, 0.100462, 0.0),
	float4(0.063233, 0.061217, 0.087974, 0.0),
	float4(0.003792, -0.022282, -0.050458, 0.0),
	float4(-0.024107, -0.010575, -0.005569, 0.0),
	float4(0.021007, 0.044164, 0.061526, 0.0),
	float4(-0.000832, 0.012470, 0.024354, 0.0)
};

ps_output main(ps_input IN)
{
	float4 albedo = ((material.usageFlags & USE_ALBEDO_TEXTURE)
		? albedoTextures[material.textureID].Sample(linearWrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	float3 N = (material.usageFlags & USE_NORMAL_TEXTURE)
		? mul(normalTextures[material.textureID].Sample(linearWrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f), IN.tbn)
		: IN.tbn[2];

	float roughness = (material.usageFlags & USE_ROUGHNESS_TEXTURE)
		? roughnessTextures[material.textureID].Sample(linearWrapSampler, IN.uv)
		: material.roughnessOverride;
	roughness = clamp(roughness, 0.01f, 0.99f);

	float metallic = (material.usageFlags & USE_METALLIC_TEXTURE)
		? metallicTextures[material.textureID].Sample(linearWrapSampler, IN.uv)
		: material.metallicOverride;
	float ao = 1.f;// (material.usageFlags & USE_AO_TEXTURE) ? RMAO.z : 1.f;


	float3 camToP = IN.worldPosition - camera.position.xyz;
	float3 V = -normalize(camToP);
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.xyz, metallic);

	float4 totalLighting = float4(0.f, 0.f, 0.f, albedo.w);


	// Ambient.
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz, irradianceTexture, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);


	// Sun.
	{
		float visibility = 1.f;
		uint numCascades = sunLight.numShadowCascades;

		if (numCascades > 0)
		{
			int currentCascadeIndex = 0;
			int nextCascadeIndex = currentCascadeIndex;
			float currentPixelsBlendBandLocation = 1.f;

			if (numCascades > 1)
			{
				float4 cascadeRelativeDistances = pow(float4(1.f, 2.f, 3.f, 4.f) / numCascades, sunLight.shadowMapCascadeDistancePower);
				float nearPlane = camera.projectionParams.x;
				float farPlane = camera.projectionParams.y;
				float4 cascadeDistances = float4(
					lerp(nearPlane, farPlane, cascadeRelativeDistances.x),
					lerp(nearPlane, farPlane, cascadeRelativeDistances.y),
					lerp(nearPlane, farPlane, cascadeRelativeDistances.z),
					lerp(nearPlane, farPlane, cascadeRelativeDistances.w)
					);

				float worldDepth = dot(camera.forward.xyz, camToP);
				float4 comparison = float4(worldDepth, worldDepth, worldDepth, worldDepth) > cascadeDistances;
				float index = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);

				index = min(index, float(numCascades) - 1.f);
				currentCascadeIndex = uint(index);
			}

			float4 lightProjected = mul(sunLight.vp[currentCascadeIndex], float4(IN.worldPosition, 1.f));
			// Since the sun is a directional (orthographic) light source, we don't need to divide by w.

			float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
			lightUV.y = 1.f - lightUV.y;

			visibility = 0.f;
			uint count = 0;

			float texelSize = 1.f / (float)sunLight.shadowMapDimensions;
			for (int y = -2; y <= 2; ++y)
			{
				for (int x = -2; x <= 2; ++x)
				{
					visibility += sunShadowMapCascades[currentCascadeIndex].SampleCmpLevelZero(shadowMapSampler, lightUV + float2(x, y) * texelSize, lightProjected.z - 0.001f);
					++count;
				}
			}
			visibility /= count;
		}

		float3 L = -sunLight.worldSpaceDirection.xyz;
		float3 radiance = sunLight.color.xyz; // No attenuation for sun.

		totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic) * visibility;
	}

#if 0
	// Point lights.
	{
		for (uint l = 0; l < 512; ++l)
		{
			point_light light = pointLights[l];

			float3 lightToP = light.worldSpacePositionAndRadius.xyz - IN.worldPosition;
			float distance = length(lightToP);
			float3 L = lightToP / distance;
			float3 radiance = light.color.xyz * saturate(1.f - distance / light.worldSpacePositionAndRadius.w); // TODO: Attenuation.

			totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
		}
	}
#endif


	ps_output OUT;
	//OUT.color = irradianceTexture.Sample(brdfSampler, N);
	//OUT.color = sampleSphericalHarmonics(shs[0], N);// totalLighting;
	OUT.color = totalLighting;
	return OUT;
}