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

	// Temporary.
	float time = 0;

	void update(uint32 width, uint32 height, float dt)
	{
		float aspectRatio = (float)width / (float)height;
		projectionMatrix = mat4::CreatePerspectiveFieldOfView(fov, aspectRatio, 0.1f, 100.0f);

		time += dt;
		while (time >= DirectX::XM_PI * 2.f)
		{
			time -= DirectX::XM_PI * 2.f;
		}

		float x = cosf(time) * 4.f;
		float z = sinf(time) * 4.f;

		position.x = x;
		position.z = z;

		//rotation = quat::CreateFromAxisAngle(vec3(0.f, 1.f, 0.f), time);


		viewMatrix = mat4::CreateLookAt(position, vec3(0.f, 1.f, 0.f), vec3(0.f, 1.f, 0.f));// (mat4::CreateFromQuaternion(rotation) * mat4::CreateTranslation(position)).Invert();
	}

	void fillConstantBuffer(camera_cb& cb)
	{
		cb.vp = viewMatrix * projectionMatrix;
		cb.v = viewMatrix;
		cb.p = projectionMatrix;
		cb.invV = viewMatrix.Invert();
		cb.invP = projectionMatrix.Invert();
		cb.invVP = cb.invP * cb.invV;
		cb.pos = vec4(position.x, position.y, position.z, 1.f);
	}
};

