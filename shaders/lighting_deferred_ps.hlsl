#include "pbr.hlsli"
#include "normals.hlsli"
#include "camera.hlsli"
#include "lighting.hlsli"

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
	totalLighting.xyz += calculateAmbientLighting(albedo, irradianceTexture, environmentTexture, brdf, brdfSampler, N, V, F0, roughness, metallic, ao);

#if 0
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

				float4 comparison = float4(worldDepth, worldDepth, worldDepth, worldDepth) > cascadeDistances;
				float index = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);

				index = min(index, float(numCascades) - 1.f);
				currentCascadeIndex = uint(index);
			}

			float4 lightProjected = mul(sunLight.vp[currentCascadeIndex], float4(worldPosition, 1.f));
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

		totalLighting.xyz += calculateDirectLighting(albedo, radiance, N, L, V, F0, roughness, metallic) * visibility;
	}
#endif

	return totalLighting;
}