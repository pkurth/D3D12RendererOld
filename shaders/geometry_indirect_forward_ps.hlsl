#include "normals.hlsli"
#include "material.hlsli"
#include "pbr.hlsli"
#include "camera.hlsli"
#include "light_probe.hlsli"

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
ConstantBuffer<directional_light> sunLight	: register(b3);


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

// Light probes.
StructuredBuffer<float4> lightProbePositions					: register(t0, space6);
StructuredBuffer<packed_spherical_harmonics> sphericalHarmonics	: register(t1, space6);
StructuredBuffer<light_probe_tetrahedron> lightProbeTetrahedra	: register(t2, space6);


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
	
#if 0
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz, irradianceTexture, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
#else
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz,
		lightProbePositions, lightProbeTetrahedra, IN.worldPosition, IN.lightProbeTetrahedron, sphericalHarmonics,
		environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);
#endif

#if 1
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
					visibility += sunShadowMapCascades[currentCascadeIndex].SampleCmpLevelZero(shadowMapSampler, lightUV + float2(x, y) * texelSize, lightProjected.z - 0.01f);
					++count;
				}
			}
			visibility /= count;
		}

		float3 L = -sunLight.worldSpaceDirection.xyz;
		float3 radiance = sunLight.color.xyz; // No attenuation for sun.

		totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic) * visibility;
	}
#endif

	ps_output OUT;
	OUT.color = totalLighting;
	//OUT.color = float4(1, 0, 0, 1);
	return OUT;
}