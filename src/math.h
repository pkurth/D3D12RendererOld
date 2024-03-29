#pragma once

#include "common.h"

// This file is a custom wrapper around DirectX's math library.
// It provides some utility functions, but most importantly it reverses the multiplication order.
// Vectors are now multiplied at the right of matrices and an MVP is build like this: P * V * M

#include <DirectXMath.h>

union vec2;
union vec3;
union vec4;
union quat;

union mat4
{
	DirectX::XMFLOAT4X4 dxmatrix;

	struct
	{
		float m00;
		float m10;
		float m20;
		float m30;

		float m01;
		float m11;
		float m21;
		float m31;

		float m02;
		float m12;
		float m22;
		float m32;

		float m03;
		float m13;
		float m23;
		float m33;
	};

	float data[16];

	inline mat4() {}
	inline mat4(const DirectX::XMFLOAT4X4A& m) { dxmatrix = m; }
	inline mat4(const DirectX::XMMATRIX& m) { DirectX::XMStoreFloat4x4(&dxmatrix, m); }
	inline mat4(float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		this->m00 = m00; this->m01 = m01; this->m02 = m02; this->m03 = m03;
		this->m10 = m10; this->m11 = m11; this->m12 = m12; this->m13 = m13;
		this->m20 = m20; this->m21 = m21; this->m22 = m22; this->m23 = m23;
		this->m30 = m30; this->m31 = m31; this->m32 = m32; this->m33 = m33;
	}

	inline operator const DirectX::XMFLOAT4X4& () const { return dxmatrix; }
	inline operator DirectX::XMMATRIX() const { return DirectX::XMLoadFloat4x4(&dxmatrix); }

	inline DirectX::XMMATRIX invert() { return DirectX::XMMatrixInverse(nullptr, *this); }

	static const mat4 identity;
};

union mat3x4
{
	DirectX::XMFLOAT3X4 dxmatrix;

	struct
	{
		float m00;
		float m10;
		float m20;

		float m01;
		float m11;
		float m21;

		float m02;
		float m12;
		float m22;

		float m03;
		float m13;
		float m23;
	};

	float data[12];

	inline mat3x4() {}
	inline mat3x4(const DirectX::XMFLOAT3X4A& m) { dxmatrix = m; }
	inline mat3x4(const DirectX::XMMATRIX& m) { DirectX::XMStoreFloat3x4(&dxmatrix, m); }
	inline mat3x4(float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23)
	{
		this->m00 = m00; this->m01 = m01; this->m02 = m02; this->m03 = m03;
		this->m10 = m10; this->m11 = m11; this->m12 = m12; this->m13 = m13;
		this->m20 = m20; this->m21 = m21; this->m22 = m22; this->m23 = m23;
	}

	inline operator const DirectX::XMFLOAT3X4& () const { return dxmatrix; }
	inline operator DirectX::XMMATRIX() const { return DirectX::XMLoadFloat3x4(&dxmatrix); }
};

union vec2
{
	DirectX::XMFLOAT2 dxvector;

	struct
	{
		float x;
		float y;
	};

	float data[2];

	inline vec2() {}
	inline vec2(float x, float y) { this->x = x; this->y = y; }
	inline vec2(const DirectX::XMFLOAT2& v) { dxvector = v; }
	inline vec2(const DirectX::XMVECTOR& v) { DirectX::XMStoreFloat2(&dxvector, v); }

	inline operator const DirectX::XMFLOAT2& () const { return dxvector; }
	inline operator DirectX::XMVECTOR() const { return DirectX::XMLoadFloat2(&dxvector); } // This initializes z and w to 0.
};

union vec3
{
	DirectX::XMFLOAT3 dxvector;

	struct
	{
		union
		{
			struct
			{
				float x;
				float y;
			};

			vec2 xy;
		};

		float z;
	};

	float data[3];

	inline vec3() {}
	inline vec3(float x, float y, float z) { this->x = x; this->y = y; this->z = z; }
	inline vec3(vec2 xy, float z) { this->xy = xy; this->z = z; }
	inline vec3(const DirectX::XMFLOAT3& v) { dxvector = v; }
	inline vec3(const DirectX::XMVECTOR& v) { DirectX::XMStoreFloat3(&dxvector, v); }

	inline operator const DirectX::XMFLOAT3& () const { return dxvector; }
	inline operator DirectX::XMVECTOR() const { return DirectX::XMLoadFloat3(&dxvector); } // This initializes w to 0.

	static const vec3 right;
	static const vec3 up;
	static const vec3 forward;
};

union vec4
{
	DirectX::XMFLOAT4 dxvector;

	struct
	{
		union
		{
			struct
			{
				float x;
				float y;
				float z;
			};

			vec3 xyz;
		};

		float w;
	};

	float data[4];

	inline vec4() {}
	inline vec4(float x, float y, float z, float w) { this->x = x; this->y = y; this->z = z; this->w = w; }
	inline vec4(vec3 xyz, float w) { this->xyz = xyz; this->w = w; }
	inline vec4(const DirectX::XMFLOAT4& v) { dxvector = v; }
	inline vec4(const DirectX::XMVECTOR& v) { DirectX::XMStoreFloat4(&dxvector, v); }

	inline operator const DirectX::XMFLOAT4& () const { return dxvector; }
	inline operator DirectX::XMVECTOR() const { return DirectX::XMLoadFloat4(&dxvector); }
};

union quat
{
	DirectX::XMFLOAT4 dxquat;

	struct
	{
		float x;
		float y;
		float z;
		float w;
	};

	float data[4];

	inline quat() {}
	inline quat(const DirectX::XMFLOAT4A& v) { dxquat = v; }
	inline quat(const DirectX::XMVECTOR& v) { DirectX::XMStoreFloat4(&dxquat, v); }
	inline quat(float x, float y, float z, float w) { this->x = x; this->y = y; this->z = z; this->w = w; }

	inline operator const DirectX::XMFLOAT4& () const { return dxquat; }
	inline operator DirectX::XMVECTOR() const { return DirectX::XMLoadFloat4(&dxquat); }

	static const quat identity;
};



struct alignas(16) comp_mat
{
	DirectX::XMMATRIX dxmatrix;

	inline comp_mat() {}
	inline comp_mat(const DirectX::XMMATRIX& m) { dxmatrix = m; }
	inline comp_mat(const mat4& m) { dxmatrix = m; }
	inline comp_mat(const mat3x4& m) { dxmatrix = m; }

	inline operator const DirectX::XMMATRIX& () const { return dxmatrix; }
	inline operator mat4() const { return dxmatrix; }

	inline comp_mat transpose() { return DirectX::XMMatrixTranspose(dxmatrix); }
	inline comp_mat invert() { return DirectX::XMMatrixInverse(nullptr, dxmatrix); }
};

struct alignas(16) comp_vec
{
	DirectX::XMVECTOR dxvector;

	inline comp_vec() {}
	inline comp_vec(const DirectX::XMVECTOR& v) { dxvector = v; }
	inline comp_vec(float x, float y, float z, float w = 0.f) { dxvector = DirectX::XMVectorSet(x, y, z, w); }
	inline comp_vec(const vec4 & v) { dxvector = v; }
	inline comp_vec(const vec3 & v) { dxvector = v; }
	inline comp_vec(const vec2 & v) { dxvector = v; }

	inline operator const DirectX::XMVECTOR& () const { return dxvector; }
	inline operator vec2() const { return dxvector; }
	inline operator vec3() const { return dxvector; }
	inline operator vec4() const { return dxvector; }

	inline comp_vec normalize() const { return DirectX::XMVector4Normalize(dxvector); }
};

struct alignas(16) comp_quat
{
	DirectX::XMVECTOR dxquat;

	inline comp_quat() {}
	inline comp_quat(const DirectX::XMVECTOR & v) { dxquat = v; }
	inline comp_quat(float x, float y, float z, float w) { dxquat = DirectX::XMVectorSet(x, y, z, w); }
	inline comp_quat(comp_vec axis, float angle) { dxquat = DirectX::XMQuaternionRotationNormal(axis, angle); } // This assumes, that the axis is normalized.
	inline comp_quat(const quat & v) { dxquat = v; }

	inline operator const DirectX::XMVECTOR& () const { return dxquat; }
	inline operator quat() const { return dxquat; }

	inline comp_quat normalize() { return DirectX::XMQuaternionNormalize(dxquat); }
};

inline comp_mat operator*(const comp_mat& a, const comp_mat& b)
{
	return DirectX::XMMatrixMultiply(b, a);
}

inline comp_vec operator*(const comp_mat& m, const comp_vec& v)
{
	return DirectX::XMVector4Transform(v, m);
}

inline comp_mat operator*(const comp_mat& m, float s)
{
	return m * DirectX::XMMatrixScaling(s, s, s);
}

inline comp_vec operator*(comp_vec v, float s)
{
	return DirectX::XMVectorScale(v, s);
}

inline comp_vec& operator*=(comp_vec& v, float s)
{
	v = v * s;
	return v;
}

inline comp_vec operator/(comp_vec v, float s)
{
	return DirectX::XMVectorScale(v, 1.f / s);
}

inline comp_vec& operator/=(comp_vec& v, float s)
{
	v = v / s;
	return v;
}

inline comp_vec operator*(float s, comp_vec v)
{
	return DirectX::XMVectorScale(v, s);
}

inline comp_vec operator+(comp_vec a, comp_vec b)
{
	return DirectX::XMVectorAdd(a, b);
}

inline comp_vec& operator+=(comp_vec& a, comp_vec b)
{
	a = a + b;
	return a;
}

inline comp_vec operator-(comp_vec a, comp_vec b)
{
	return DirectX::XMVectorSubtract(a, b);
}

inline comp_vec operator-(comp_vec v)
{
	return comp_vec(0.f, 0.f, 0.f, 0.f) - v;
}

inline comp_vec& operator-=(comp_vec& a, comp_vec b)
{
	a = a - b;
	return a;
}

inline comp_quat operator*(const comp_quat& q1, const comp_quat& q2)
{
	return DirectX::XMQuaternionMultiply(q2, q1);
}

inline comp_vec operator*(const comp_quat& q, const comp_vec& v)
{
	return DirectX::XMVector3Rotate(v, q);
}

inline comp_quat slerp(const comp_quat& q1, const comp_quat& q2, float t)
{
	return DirectX::XMQuaternionSlerp(q1, q2, t);
}

inline comp_mat createLookAt(const comp_vec& eye, const comp_vec& focus, const comp_vec& up)
{
	return DirectX::XMMatrixLookAtRH(eye, focus, up);
}

inline comp_mat createPerspectiveMatrix(float fovY, float aspect, float nearPlane, float farPlane = -1.f)
{
	return DirectX::XMMatrixPerspectiveFovRH(fovY, aspect, nearPlane, farPlane);
}

inline comp_mat createOrthographicMatrix(float left, float right, float top, float bottom, float nearPlane, float farPlane)
{
	return DirectX::XMMatrixOrthographicOffCenterRH(left, right, bottom, top, nearPlane, farPlane);
}

inline comp_mat createScaleMatrix(float scale)
{
	return DirectX::XMMatrixScaling(scale, scale, scale);
}

inline comp_mat createScaleMatrix(float scaleX, float scaleY, float scaleZ)
{
	return DirectX::XMMatrixScaling(scaleX, scaleY, scaleZ);
}

inline comp_mat createTranslationMatrix(float transX, float transY, float transZ)
{
	return DirectX::XMMatrixTranslation(transX, transY, transZ);
}

inline comp_mat createTranslationMatrix(comp_vec trans)
{
	return DirectX::XMMatrixTranslationFromVector(trans);
}

inline comp_mat createModelMatrix(comp_vec trans, comp_quat rot, float scale = 1.f)
{
	return DirectX::XMMatrixAffineTransformation(comp_vec(scale, scale, scale), comp_vec(0.f, 0.f, 0.f), rot, trans);
}

inline comp_quat createQuaternionFromAxisAngle(comp_vec axis, float angle)
{
	return DirectX::XMQuaternionRotationAxis(axis, angle);
}

inline float dot2(comp_vec a, comp_vec b)
{
	return DirectX::XMVector2Dot(a, b).m128_f32[0];
}

inline float dot3(comp_vec a, comp_vec b)
{
	return DirectX::XMVector3Dot(a, b).m128_f32[0];
}

inline float dot4(comp_vec a, comp_vec b)
{
	return DirectX::XMVector4Dot(a, b).m128_f32[0];
}

inline comp_vec cross(comp_vec a, comp_vec b)
{
	return DirectX::XMVector3Cross(a, b);
}

struct trs
{
	quat rotation;
	vec3 position;
	float scale;

	trs() {}

	trs(vec3 position, quat rotation, float scale = 1.f)
		: position(position), rotation(rotation), scale(scale) { }

	explicit trs(const mat4& m)
	{
		vec3 c0(m.m00, m.m10, m.m20);
		scale = sqrt(dot3(c0, c0));
		float invScale = 1.f / scale;

		position.x = m.m03;
		position.y = m.m13;
		position.z = m.m23;

		mat4 R = m;
		R.m03 = R.m13 = R.m23 = 0.f;
		R = R * invScale;

		float tr = m.m00 + m.m11 + m.m22;

		if (tr > 0.f)
		{
			float s = sqrtf(tr + 1.f) * 2.f; // S=4*qw 
			rotation.w = 0.25f * s;
			rotation.x = (R.m21 - R.m12) / s;
			rotation.y = (R.m02 - R.m20) / s;
			rotation.z = (R.m10 - R.m01) / s;
		}
		else if ((R.m00 > R.m11) && (R.m00 > R.m22))
		{
			float s = sqrtf(1.f + R.m00 - R.m11 - R.m22) * 2.f; // S=4*qx 
			rotation.w = (R.m21 - R.m12) / s;
			rotation.x = 0.25f * s;
			rotation.y = (R.m01 + R.m10) / s;
			rotation.z = (R.m02 + R.m20) / s;
		}
		else if (R.m11 > R.m22)
		{
			float s = sqrtf(1.f + R.m11 - R.m00 - R.m22) * 2.f; // S=4*qy
			rotation.w = (R.m02 - R.m20) / s;
			rotation.x = (R.m01 + R.m10) / s;
			rotation.y = 0.25f * s;
			rotation.z = (R.m12 + R.m21) / s;
		}
		else
		{
			float s = sqrtf(1.f + R.m22 - R.m00 - R.m11) * 2.f; // S=4*qz
			rotation.w = (R.m10 - R.m01) / s;
			rotation.x = (R.m02 + R.m20) / s;
			rotation.y = (R.m12 + R.m21) / s;
			rotation.z = 0.25f * s;
		}
	}

	static const trs identity;
};

inline trs operator*(const trs& a, const trs& b)
{
	trs result;
	result.position = a.rotation * (a.scale * b.position) + a.position;
	result.rotation = a.rotation * b.rotation;
	result.scale = a.scale * b.scale;
	return result;
}

template <typename T>
inline T bucketize(T problemSize, size_t bucketSize)
{
	return (T)((problemSize + bucketSize - 1) / bucketSize);
}

template <typename T>
inline T lerp(T lo, T hi, float t)
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

struct bounding_box
{
	vec3 min;
	vec3 max;

	static bounding_box negativeInfinity()
	{
		bounding_box result = { vec3(FLT_MAX, FLT_MAX, FLT_MAX), vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX) };
		return result;
	}

	void grow(vec3 a)
	{
		min.x = ::min(min.x, a.x);
		min.y = ::min(min.y, a.y);
		min.z = ::min(min.z, a.z);
		max.x = ::max(max.x, a.x);
		max.y = ::max(max.y, a.y);
		max.z = ::max(max.z, a.z);
	}

	void expand(float v)
	{
		min = min - vec3(v, v, v);
		max = max + vec3(v, v, v);
	}

	bool intersectSphere(vec3 position, float radius) const;
};

struct ray
{
	vec3 origin;
	vec3 direction;

	bool intersectPlane(vec4 plane, float& t) const;
	bool intersectAABB(const bounding_box& aabb, float& t) const;
};

#define padded_sizeof(str, paddedTo) ((sizeof(str) + (paddedTo - 1))& ~(paddedTo - 1))



