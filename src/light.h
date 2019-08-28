#pragma once

#include "common.h"
#include "math.h"

#include "camera.h"


#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f

struct directional_light
{
	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 worldSpaceDirection;
	vec4 color;

	uint32 numShadowCascades = 2;

	void updateMatrices(const camera_frustum& cameraWorldSpaceFrustum)
	{
		comp_mat viewMatrix = createLookAt(vec3(0.f, 0.f, 0.f), worldSpaceDirection, vec3(0.f, 1.f, 0.f));

		comp_vec nearBottomLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.nearBottomLeft, 1.f);
		comp_vec nearBottomRight = viewMatrix * vec4(cameraWorldSpaceFrustum.nearBottomRight, 1.f);
		comp_vec nearTopLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.nearTopLeft, 1.f);
		comp_vec nearTopRight = viewMatrix * vec4(cameraWorldSpaceFrustum.nearTopRight, 1.f);
		comp_vec farBottomLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.farBottomLeft, 1.f);
		comp_vec farBottomRight = viewMatrix * vec4(cameraWorldSpaceFrustum.farBottomRight, 1.f);
		comp_vec farTopLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.farTopLeft, 1.f);
		comp_vec farTopRight = viewMatrix * vec4(cameraWorldSpaceFrustum.farTopRight, 1.f);

		bounding_box bb = bounding_box::negativeInfinity();
		bb.grow(nearBottomLeft);
		bb.grow(nearBottomRight);
		bb.grow(nearTopLeft);
		bb.grow(nearTopRight);

		for (uint32 i = 0; i < numShadowCascades; ++i)
		{
			float distance = powf((float)(i + 1) / numShadowCascades, 2.f);

			bb.grow(lerp(nearBottomLeft, farBottomLeft, distance));
			bb.grow(lerp(nearBottomRight, farBottomRight, distance));
			bb.grow(lerp(nearTopLeft, farTopLeft, distance));
			bb.grow(lerp(nearTopRight, farTopRight, distance));

			comp_mat projMatrix = createOrthographicMatrix(bb.min.x, bb.max.x, bb.max.y, bb.min.y, -bb.max.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -bb.min.z);

			vp[i] = projMatrix * viewMatrix;
		}
	}
};

