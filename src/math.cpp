#include "pch.h"
#include "math.h"

const mat4 mat4::identity = { 1.f, 0.f, 0.f, 0.f,
							0.f, 1.f, 0.f, 0.f,
							0.f, 0.f, 1.f, 0.f,
							0.f, 0.f, 0.f, 1.f };

const quat quat::identity = { 0.f, 0.f, 0.f, 1.f };

const trs trs::identity = trs(vec3(0.f, 0.f, 0.f), quat::identity, 1.f);

const vec3 vec3::right = { 1.f, 0.f, 0.f };
const vec3 vec3::up = { 0.f, 1.f, 0.f };
const vec3 vec3::forward = { 0.f, 0.f, -1.f };

bool ray::intersectPlane(vec4 plane, float& t) const
{
	float nDotR = dot3(direction, plane);
	if (fabsf(nDotR) <= 1e-6f)
	{
		return false;
	}

	vec4 o(origin, 1.f);
	t = -dot4(o, plane) / nDotR;

	return t > 0.f;
}

bool ray::intersectAABB(const bounding_box& aabb, float& t) const
{
	vec3 invDir = vec3(1.f / direction.x, 1.f / direction.y, 1.f / direction.z); // This can be NaN but still works.

	float tx1 = (aabb.min.x - origin.x) * invDir.x;
	float tx2 = (aabb.max.x - origin.x) * invDir.x;

	t = min(tx1, tx2);
	float tmax = max(tx1, tx2);

	float ty1 = (aabb.min.y - origin.y) * invDir.y;
	float ty2 = (aabb.max.y - origin.y) * invDir.y;

	t = max(t, min(ty1, ty2));
	tmax = min(tmax, max(ty1, ty2));

	float tz1 = (aabb.min.z - origin.z) * invDir.z;
	float tz2 = (aabb.max.z - origin.z) * invDir.z;

	t = max(t, min(tz1, tz2));
	tmax = min(tmax, max(tz1, tz2));

	bool result = tmax >= t && t > 0.f;

	return result;
}

bool bounding_box::intersectSphere(vec3 position, float radius) const
{
	float sqDist = 0.f;
	for (int i = 0; i < 3; ++i)
	{
		// For each axis count any excess distance outside box extents
		float v = position.data[i];
		if (v < min.data[i]) sqDist += (min.data[i] - v) * (min.data[i] - v);
		if (v > max.data[i]) sqDist += (v - max.data[i]) * (v - max.data[i]);
	}

	bool intersection = sqDist <= radius * radius;
	return intersection;
}
