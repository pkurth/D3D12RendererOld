#pragma once

#include "common.h"
#include "math.h"

#include "camera.h"

struct directional_light
{
	vec4 worldSpaceDirection;
	vec4 color;

	mat4 vp;

	void updateMatrices(const camera_frustum& cameraWorldSpaceFrustum)
	{
		comp_mat viewMatrix = createLookAt(vec3(0.f, 0.f, 0.f), worldSpaceDirection, vec3(0.f, 1.f, 0.f));

		bounding_box bb = bounding_box::negativeInfinity();
		for (uint32 i = 0; i < arraysize(cameraWorldSpaceFrustum.corners); ++i)
		{
			bb.grow(viewMatrix * cameraWorldSpaceFrustum.corners[i]);
		}

		comp_mat projMatrix = createOrthographicMatrix(bb.min.x, bb.max.x, bb.max.y, bb.min.y, -bb.max.z, -bb.min.z);

		vp = projMatrix * viewMatrix;
	}
};

