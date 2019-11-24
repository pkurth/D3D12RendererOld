#pragma once

#include "common.h"
#include "math.h"

union camera_frustum
{
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

	camera_frustum() {}
};

struct camera_cb
{
	mat4 vp;
	mat4 v;
	mat4 p;
	mat4 invVP;
	mat4 invV;
	mat4 invP;
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


	void updateMatrices(uint32 width, uint32 height)
	{
		float aspectRatio = (float)width / (float)height;
		projectionMatrix = createPerspectiveMatrix(fovY, aspectRatio, nearPlane, farPlane);
		invProjectionMatrix = projectionMatrix.invert();

		viewMatrix = createModelMatrix(position, rotation).invert();
		invViewMatrix = viewMatrix.invert();

		invViewProjectionMatrix = invViewMatrix * invProjectionMatrix;
	}

	void fillConstantBuffer(camera_cb& cb)
	{
		cb.vp = projectionMatrix * viewMatrix;
		cb.v = viewMatrix;
		cb.p = projectionMatrix;
		cb.invV = invViewMatrix;
		cb.invP = invProjectionMatrix;
		cb.invVP = invViewProjectionMatrix;
		cb.pos = vec4(position.x, position.y, position.z, 1.f);
		cb.forward = rotation * vec3(0.f, 0.f, -1.f);
		cb.projectionParams = vec4(nearPlane, farPlane, farPlane / nearPlane, 1.f - farPlane / nearPlane);
	}

	comp_vec restoreViewSpacePosition(vec2 uv, float depthBufferDepth)
	{
		uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
		vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
		comp_vec homPosition = invProjectionMatrix * vec4(ndc, 1.f);
		comp_vec position = homPosition / homPosition.dxvector.m128_f32[3];
		return position;
	}

	comp_vec restoreWorldSpacePosition(vec2 uv, float depthBufferDepth)
	{
		uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
		vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
		comp_vec homPosition = invViewProjectionMatrix * vec4(ndc, 1.f);
		comp_vec position = homPosition / homPosition.dxvector.m128_f32[3];
		return position;
	}

	float depthBufferDepthToLinearNormalizedDepthEyeToFarPlane(float depthBufferDepth)
	{
		float c1 = farPlane / nearPlane;
		float c0 = 1.f - c1;
		return 1.f / (c0 * depthBufferDepth + c1);
	}

	camera_frustum getWorldSpaceFrustum()
	{
		camera_frustum result;

		result.nearBottomLeft = restoreWorldSpacePosition(vec2(0.f, 1.f), 0.f);
		result.nearBottomRight = restoreWorldSpacePosition(vec2(1.f, 1.f), 0.f);
		result.nearTopLeft = restoreWorldSpacePosition(vec2(0.f, 0.f), 0.f);
		result.nearTopRight = restoreWorldSpacePosition(vec2(1.f, 0.f), 0.f);
		result.farBottomLeft = restoreWorldSpacePosition(vec2(0.f, 1.f), 1.f);
		result.farBottomRight = restoreWorldSpacePosition(vec2(1.f, 1.f), 1.f);
		result.farTopLeft = restoreWorldSpacePosition(vec2(0.f, 0.f), 1.f);
		result.farTopRight = restoreWorldSpacePosition(vec2(1.f, 0.f), 1.f);

		return result;
	}
};

struct cubemap_camera : render_camera
{
	void initialize(vec3 position, uint32 cubemapIndex)
	{
		this->position = position;

		switch (cubemapIndex)
		{
		case 0: this->rotation = createQuaternionFromAxisAngle(vec3::up, DirectX::XMConvertToRadians(-90.f)); break;	// +X.
		case 1: this->rotation = createQuaternionFromAxisAngle(vec3::up, DirectX::XMConvertToRadians(90.f)); break;		// -X.
		case 2: this->rotation = createQuaternionFromAxisAngle(vec3::right, DirectX::XMConvertToRadians(90.f)); break;	// +Y.
		case 3: this->rotation = createQuaternionFromAxisAngle(vec3::right, DirectX::XMConvertToRadians(-90.f)); break; // -Y.
		case 4: this->rotation = createQuaternionFromAxisAngle(vec3::up, DirectX::XMConvertToRadians(180.f)); break;	// +Z.
		case 5: this->rotation = quat::identity; break; 																// -Z.
		default: assert(false);
		}

		this->fovY = DirectX::XMConvertToRadians(90.f);
		this->nearPlane = 0.1f;
		this->farPlane = 1000.f;

		updateMatrices(1, 1);
	}
};

