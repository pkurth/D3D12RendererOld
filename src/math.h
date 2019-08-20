#pragma once


#include <dx/SimpleMath.h>

using vec2 = DirectX::SimpleMath::Vector2;
using vec3 = DirectX::SimpleMath::Vector3;
using vec4 = DirectX::SimpleMath::Vector4;
using mat4 = DirectX::SimpleMath::Matrix;
using quat = DirectX::SimpleMath::Quaternion;


template <typename T>
inline T bucketize(T value, size_t alignment)
{
	return (T)((value + alignment - 1) / alignment);
}

template <typename T>
inline T lerp(T lo, T hi, T t)
{
	return lo + (hi - lo) * t;
}

inline float inverseLerp(float lo, float hi, float x)
{
	return (x - lo) / (hi - lo);
}

inline float remap(float x, float curLo, float curHi, float newLo, float newHi)
{
	return lerp(newLo, newHi, inverseLerp(curLo, curHi, x));
}

inline float randomFloat(float lowerBound, float upperBound)
{
	return remap((float)rand(), 0.f, (float)RAND_MAX, lowerBound, upperBound);
}

inline uint32 randomUint(uint32 lowerBound, uint32 upperBound)
{
	return (rand() % (upperBound - lowerBound)) + lowerBound;
}

#define padded_sizeof(str, paddedTo) ((sizeof(str) + (paddedTo - 1)) & ~(paddedTo - 1))



