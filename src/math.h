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

#define padded_sizeof(str, paddedTo) ((sizeof(str) + (paddedTo - 1)) & ~(paddedTo - 1))



