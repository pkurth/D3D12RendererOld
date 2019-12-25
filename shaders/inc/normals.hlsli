#ifndef NORMALS_H
#define NORMALS_H


float2 signNotZero(float2 v) 
{
	return float2(mad(step(0.f, v.x), 2.f, -1.f),
				  mad(step(0.f, v.y), 2.f, -1.f));
}

float2 encodeNormal(float3 v)
{
	v.xy /= dot(abs(v), float3(1.f, 1.f, 1.f));
	return lerp(v.xy, (float2(1.f, 1.f) - abs(v.yx)) * signNotZero(v.xy), step(v.z, 0.f));
}

float3 decodeNormal(float2 packedNormal)
{
	float3 v = float3(packedNormal.xy, 1.f - abs(packedNormal.x) - abs(packedNormal.y));
	if (v.z < 0.f) v.xy = (1.f - abs(v.yx)) * signNotZero(v.xy);
	return normalize(v);
}

#endif
