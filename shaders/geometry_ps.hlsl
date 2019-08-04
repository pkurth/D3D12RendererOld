#include "normals.h"

struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3x3 tbn    : TBN;
};

struct ps_output
{
	float4 albedo	: SV_Target0;
	float4 emission : SV_Target1;
	float4 normal	: SV_Target2;
};

SamplerState texSampler			: register(s0);
Texture2D<float4> albedo		: register(t0);
Texture2D<float4> normal		: register(t1);
Texture2D<float4> roughMetal	: register(t2);

ps_output main(ps_input IN)
{
	ps_output OUT;

	float4 color = albedo.Sample(texSampler, IN.uv);
	if (color.a < 0.01f)
	{
		discard;
	}

	float3 N = mul(normal.Sample(texSampler, IN.uv).xyz, IN.tbn); // Multiply from the right, because the mat3 constructor takes rows.
	float2 RM = roughMetal.Sample(texSampler, IN.uv).xy;

	OUT.albedo = color;
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(encodeNormal(normalize(N)), RM.xy);

	return OUT;
}
