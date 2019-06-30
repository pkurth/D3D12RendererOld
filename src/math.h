#pragma once

#include <DirectXMath.h>

typedef DirectX::XMFLOAT2 vec2;
typedef DirectX::XMFLOAT3 vec3;
typedef DirectX::XMVECTOR vec4;
typedef DirectX::XMMATRIX mat4;


template <typename T>
inline T bucketize(T value, size_t alignment)
{
	return (T)((value + alignment - 1) / alignment);
}
