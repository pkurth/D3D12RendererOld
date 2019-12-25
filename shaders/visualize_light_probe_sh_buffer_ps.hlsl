#include "light_probe.h"

struct ps_input
{
	float3 uv	: TEXCOORDS;
};

StructuredBuffer<spherical_harmonics> shs	: register(t0);

cbuffer index_cb : register(b1)
{
	uint shIndex;
};

float4 main(ps_input IN) : SV_TARGET
{
	return sampleSphericalHarmonics(shs[shIndex], IN.uv);
}
