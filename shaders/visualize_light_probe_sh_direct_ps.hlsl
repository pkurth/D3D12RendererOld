#include "light_probe.h"

struct ps_input
{
	float3 uv	: TEXCOORDS;
};

cbuffer sh_cb : register(b1)
{
	spherical_harmonics sh;
};

float4 main(ps_input IN) : SV_TARGET
{
	return sampleSphericalHarmonics(sh, IN.uv);
}
