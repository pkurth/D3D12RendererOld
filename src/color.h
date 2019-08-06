#pragma once

#include "common.h"
#include "math.h"

inline float srgbToLinear(float c)
{
	return c < 0.04045f ? c / 12.92f : pow(abs(c + 0.055f) / 1.055f, 2.4f);
}

float linearToSRGB(float c)
{
	return c < 0.0031308f ? 12.92f * c : 1.055f * pow(abs(c), 1.f / 2.4f) - 0.055f;
}

inline uint32 srgb1ToLinearU255(float r, float g, float b, float a = 1.f)
{
	uint32 ri = (uint32)(clamp01(srgbToLinear(r)) * 255);
	uint32 gi = (uint32)(clamp01(srgbToLinear(g)) * 255);
	uint32 bi = (uint32)(clamp01(srgbToLinear(b)) * 255);
	uint32 ai = (uint32)(a * 255);

	return (ai << 24) | (bi << 16) | (gi << 8) | (ri);
}

inline uint32 srgb1ToLinearU255(vec4 c)
{
	return srgb1ToLinearU255(c.x, c.y, c.z, c.w);
}

inline vec4 linearU255ToSRGB1(uint32 c)
{
	float r = linearToSRGB((c & 0xFF) / 255.f);
	float g = linearToSRGB(((c >> 8) & 0xFF) / 255.f);
	float b = linearToSRGB(((c >> 16) & 0xFF) / 255.f);
	float a = ((c >> 24) & 0xFF) / 255.f;
	return vec4(r, g, b, a);
}



