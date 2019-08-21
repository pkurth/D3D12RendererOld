#include "normals.h"

struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3x3 tbn    : TBN;
};

struct ps_output
{
	float4 albedoAO	: SV_Target0;
	float4 emission : SV_Target1;
	float4 normal	: SV_Target2;
};

struct material_id_cb
{
	uint id;
};

ConstantBuffer<material_id_cb> material : register(b2);

SamplerState linearWrapSampler				: register(s0);
Texture2D<float4> materialTextures[1024]	: register(t0);

ps_output main(ps_input IN)
{
	ps_output OUT;

	float4 color = materialTextures[(material.id * 3) + 0].Sample(linearWrapSampler, IN.uv);
	float3 N = materialTextures[(material.id * 3) + 1].Sample(linearWrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f);
	N = mul(N, IN.tbn); // Multiply from the left, because the mat3 constructor takes rows.
	float3 RMAO = materialTextures[(material.id * 3) + 2].Sample(linearWrapSampler, IN.uv).xyz;


	OUT.albedoAO = float4(color.xyz, RMAO.z);
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(encodeNormal(normalize(N)), RMAO.xy);

	return OUT;
}
