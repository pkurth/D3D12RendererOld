#include "normals.h"

struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3 normal	: NORMAL;
};

struct ps_output
{
	float4 albedoAO	: SV_Target0;
	float4 emission : SV_Target1;
	float4 normal	: SV_Target2;
};

SamplerState linearWrapSampler				: register(s0);
Texture2D<float4> materialTextures[1024]	: register(t0);

ps_output main(ps_input IN)
{
	ps_output OUT;

	float4 color = materialTextures[0].Sample(linearWrapSampler, IN.uv);
	float3 RMAO = materialTextures[2].Sample(linearWrapSampler, IN.uv).xyz;

	OUT.albedoAO = float4(color.xyz, RMAO.z);
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(encodeNormal(normalize(IN.normal)), RMAO.xy);

	return OUT;
}
