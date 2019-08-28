#include "normals.h"
#include "material.h"
#include "quaternion.h"

struct ps_input
{
	float2 uv		: TEXCOORDS;
	quat tbn		: TANGENT_FRAME;
};

struct ps_output
{
	float4 albedoAO	: SV_Target0;
	float4 emission : SV_Target1;
	float4 normal	: SV_Target2;
};

ConstantBuffer<material_cb> material : register(b2);

SamplerState linearWrapSampler			: register(s0);
Texture2D<float4> albedoTextures[64]	: register(t0, space0);
Texture2D<float4> normalTextures[64]	: register(t0, space1);
Texture2D<float> roughnessTextures[64]	: register(t0, space2);
Texture2D<float> metallicTextures[64]	: register(t0, space3);

uint wang_hash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

ps_output main(ps_input IN)
{
	ps_output OUT;

	float4 color = ((material.usageFlags & USE_ALBEDO_TEXTURE) 
		? albedoTextures[material.textureID].Sample(linearWrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	/*float4 color;
	uint a = wang_hash(material.drawID);
	uint b = wang_hash(a);
	uint c = wang_hash(b);
	color.x = a / 4294967296.f;
	color.y = b / 4294967296.f;
	color.z = c / 4294967296.f;*/


	float3 N = quatRotate((material.usageFlags & USE_NORMAL_TEXTURE)
		? normalize(normalTextures[material.textureID].Sample(linearWrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f))
		: float3(0.f, 0.f, 1.f),
		normalize(IN.tbn));

	//float3 RMAO = rmaoTextures[material.textureID].Sample(linearWrapSampler, IN.uv).xyz;

	float roughness = (material.usageFlags & USE_ROUGHNESS_TEXTURE) 
		? roughnessTextures[material.textureID].Sample(linearWrapSampler, IN.uv) 
		: material.roughnessOverride;

	float metallic = (material.usageFlags & USE_METALLIC_TEXTURE) 
		? metallicTextures[material.textureID].Sample(linearWrapSampler, IN.uv)
		: material.metallicOverride;
	float ao = 1.f;// (material.usageFlags & USE_AO_TEXTURE) ? RMAO.z : 1.f;

	OUT.albedoAO = float4(color.xyz, ao);
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(encodeNormal(N), roughness, metallic);

	return OUT;
}
