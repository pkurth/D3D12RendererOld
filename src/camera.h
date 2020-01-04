#pragma once

#include "common.h"
#include "math.h"

union camera_frustum_corners
{
	vec3 eye;

	struct
	{
		vec3 nearTopLeft;
		vec3 nearTopRight;
		vec3 nearBottomLeft;
		vec3 nearBottomRight;
		vec3 farTopLeft;
		vec3 farTopRight;
		vec3 farBottomLeft;
		vec3 farBottomRight;
	};
	struct
	{
		vec3 corners[8];
	};

	camera_frustum_corners() {}
};

union camera_frustum_planes
{
	struct
	{
		vec4 left;
		vec4 right;
		vec4 top;
		vec4 bottom;
		vec4 near;
		vec4 far;
	};
	vec4 planes[6];

	camera_frustum_planes() {}

	// Returns true, if object should be culled.
	bool cullWorldSpaceAABB(const bounding_box& aabb) const;
	bool cullModelSpaceAABB(const bounding_box& aabb, const mat4& transform) const;
};

struct camera_cb
{
	mat4 vp;
	mat4 v;
	mat4 p;
	mat4 invVP;
	mat4 invV;
	mat4 invP;
	mat4 skyVP;
	vec4 pos;
	vec4 forward;
	vec4 projectionParams; // nearPlane, farPlane, farPlane / nearPlane, 1 - farPlane / nearPlane
};

struct render_camera
{
	quat rotation;
	vec3 position;
	float fovY;

	float nearPlane;
	float farPlane;

	float pitch;
	float yaw;

	mat4 projectionMatrix;
	mat4 viewMatrix;

	mat4 invViewMatrix;
	mat4 invProjectionMatrix;
	mat4 invViewProjectionMatrix;

	mat4 viewProjectionMatrix;


	void updateMatrices(uint32 width, uint32 height);
	void fillConstantBuffer(camera_cb& cb) const;
	comp_vec restoreViewSpacePosition(vec2 uv, float depthBufferDepth) const;
	comp_vec restoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const;
	float depthBufferDepthToLinearNormalizedDepthEyeToFarPlane(float depthBufferDepth) const;
	float eyeDepthToDepthBufferDepth(float eyeDepth) const;
	camera_frustum_corners getWorldSpaceFrustumCorners(float alternativeFarPlane = 0.f) const;
	camera_frustum_planes getWorldSpaceFrustumPlanes() const;
};

struct cubemap_camera : render_camera
{
	void initialize(vec3 position, uint32 cubemapIndex);
};

