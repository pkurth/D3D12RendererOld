#pragma once

#include "common.h"
#include "math.h"

struct render_camera
{
	quat rotation;
	vec3 position;
	float fov;

	mat4 projectionMatrix;
	mat4 viewMatrix;

	void update(uint32 width, uint32 height, float dt)
	{
		float aspectRatio = (float)width / (float)height;
		projectionMatrix = mat4::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(70.f), aspectRatio, 0.1f, 100.0f);

		viewMatrix = (mat4::CreateFromQuaternion(rotation) * mat4::CreateTranslation(position)).Invert();
	}
};
