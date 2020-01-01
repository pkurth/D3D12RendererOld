#include "normals.hlsli"
#include "material.hlsli"

struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3x3 tbn	: TANGENT_FRAME;
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

ps_output main(ps_input IN)
{
	uint textureID = material.textureID_usageFlags >> 16;
	uint usageFlags = material.textureID_usageFlags & 0xFFFF;

	float4 color = ((usageFlags & USE_ALBEDO_TEXTURE) 
		? albedoTextures[textureID].Sample(linearWrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	float3 N = (usageFlags & USE_NORMAL_TEXTURE)
		? mul(normalTextures[textureID].Sample(linearWrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f), IN.tbn)
		: IN.tbn[2];

	float roughness = (usageFlags & USE_ROUGHNESS_TEXTURE) 
		? roughnessTextures[textureID].Sample(linearWrapSampler, IN.uv) 
		: material.roughnessOverride;

	float metallic = (usageFlags & USE_METALLIC_TEXTURE) 
		? metallicTextures[textureID].Sample(linearWrapSampler, IN.uv)
		: material.metallicOverride;
	float ao = 1.f;// (material.usageFlags & USE_AO_TEXTURE) ? RMAO.z : 1.f;

	ps_output OUT;
	OUT.albedoAO = float4(color.xyz, ao);
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(encodeNormal(N), roughness, metallic);
	return OUT;
}
