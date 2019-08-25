#pragma once

#include "common.h"
#include "math.h"


struct camera_cb
{
	mat4 vp;
	mat4 v;
	mat4 p;
	mat4 invVP;
	mat4 invV;
	mat4 invP;
	vec4 pos;
};

struct render_camera
{
	quat rotation;
	vec3 position;
	float fov;

	mat4 projectionMatrix;
	mat4 viewMatrix;

	void updateMatrices(uint32 width, uint32 height)
	{
		float aspectRatio = (float)width / (float)height;
		projectionMatrix = createPerspectiveMatrix(fov, aspectRatio, 0.1f, 100.0f);

		viewMatrix = createLookAt(position, vec3(0.f, position.y, 0.f), vec3(0.f, 1.f, 0.f));// (mat4::CreateFromQuaternion(rotation) * mat4::CreateTranslation(position)).Invert();
	}

	void fillConstantBuffer(camera_cb& cb)
	{
		cb.vp = projectionMatrix * viewMatrix;
		cb.v = viewMatrix;
		cb.p = projectionMatrix;
		cb.invV = viewMatrix.invert();
		cb.invP = projectionMatrix.invert();
		cb.invVP = cb.invV * cb.invP;
		cb.pos = vec4(position.x, position.y, position.z, 1.f);
	}
};

