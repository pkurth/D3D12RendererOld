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

ps_output main(ps_input IN)
{
	ps_output OUT;

	float4 color = float4(1.f, 0.f, 0.f, 1.f);

	OUT.albedoAO = float4(color.xyz, 1.f);
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(encodeNormal(normalize(IN.normal)), float2(1.f, 0.f));

	return OUT;
}
