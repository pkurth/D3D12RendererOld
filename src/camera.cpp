#include "pch.h"
#include "camera.h"


void render_camera::updateMatrices(uint32 width, uint32 height)
{
	float aspectRatio = (float)width / (float)height;
	projectionMatrix = createPerspectiveMatrix(fovY, aspectRatio, nearPlane, farPlane);
	invProjectionMatrix = projectionMatrix.invert();

	viewMatrix = createModelMatrix(position, rotation).invert();
	invViewMatrix = viewMatrix.invert();

	viewProjectionMatrix = projectionMatrix * viewMatrix;
	invViewProjectionMatrix = invViewMatrix * invProjectionMatrix;
}

void render_camera::fillConstantBuffer(camera_cb& cb) const
{
	cb.vp = viewProjectionMatrix;
	cb.v = viewMatrix;
	cb.p = projectionMatrix;
	cb.invV = invViewMatrix;
	cb.invP = invProjectionMatrix;
	cb.invVP = invViewProjectionMatrix;
	cb.pos = vec4(position.x, position.y, position.z, 1.f);
	cb.forward = rotation * vec3(0.f, 0.f, -1.f);
	cb.projectionParams = vec4(nearPlane, farPlane, farPlane / nearPlane, 1.f - farPlane / nearPlane);

	mat4 v = viewMatrix;
	v.m03 = 0.f; v.m13 = 0.f; v.m23 = 0.f;
	cb.skyVP = projectionMatrix * v;
}

comp_vec render_camera::restoreViewSpacePosition(vec2 uv, float depthBufferDepth) const
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
	comp_vec homPosition = invProjectionMatrix * vec4(ndc, 1.f);
	comp_vec position = homPosition / homPosition.dxvector.m128_f32[3];
	return position;
}

comp_vec render_camera::restoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
	comp_vec homPosition = invViewProjectionMatrix * vec4(ndc, 1.f);
	comp_vec position = homPosition / homPosition.dxvector.m128_f32[3];
	return position;
}

float render_camera::depthBufferDepthToLinearNormalizedDepthEyeToFarPlane(float depthBufferDepth) const
{
	float c1 = farPlane / nearPlane;
	float c0 = 1.f - c1;
	return 1.f / (c0 * depthBufferDepth + c1);
}

float render_camera::eyeDepthToDepthBufferDepth(float eyeDepth) const
{
	return -projectionMatrix.m22 + projectionMatrix.m23 / eyeDepth;
}

camera_frustum_corners render_camera::getWorldSpaceFrustumCorners(float alternativeFarPlane) const
{
	if (alternativeFarPlane <= 0.f)
	{
		alternativeFarPlane = farPlane;
	}

	float depthValue = eyeDepthToDepthBufferDepth(alternativeFarPlane);

	camera_frustum_corners result;

	result.eye = position;

	result.nearBottomLeft = restoreWorldSpacePosition(vec2(0.f, 1.f), 0.f);
	result.nearBottomRight = restoreWorldSpacePosition(vec2(1.f, 1.f), 0.f);
	result.nearTopLeft = restoreWorldSpacePosition(vec2(0.f, 0.f), 0.f);
	result.nearTopRight = restoreWorldSpacePosition(vec2(1.f, 0.f), 0.f);
	result.farBottomLeft = restoreWorldSpacePosition(vec2(0.f, 1.f), depthValue);
	result.farBottomRight = restoreWorldSpacePosition(vec2(1.f, 1.f), depthValue);
	result.farTopLeft = restoreWorldSpacePosition(vec2(0.f, 0.f), depthValue);
	result.farTopRight = restoreWorldSpacePosition(vec2(1.f, 0.f), depthValue);

	return result;
}

camera_frustum_planes render_camera::getWorldSpaceFrustumPlanes() const
{
	camera_frustum_planes result;

	vec4 c0(viewProjectionMatrix.m00, viewProjectionMatrix.m01, viewProjectionMatrix.m02, viewProjectionMatrix.m03);
	vec4 c1(viewProjectionMatrix.m10, viewProjectionMatrix.m11, viewProjectionMatrix.m12, viewProjectionMatrix.m13);
	vec4 c2(viewProjectionMatrix.m20, viewProjectionMatrix.m21, viewProjectionMatrix.m22, viewProjectionMatrix.m23);
	vec4 c3(viewProjectionMatrix.m30, viewProjectionMatrix.m31, viewProjectionMatrix.m32, viewProjectionMatrix.m33);

	result.left = c3 + c0;
	result.right = c3 - c0;
	result.top = c3 - c1;
	result.bottom = c3 + c1;
	result.near = c2;
	result.far = c3 - c2;

	return result;
}

void cubemap_camera::initialize(vec3 position, uint32 cubemapIndex)
{
	this->position = position;

	// We flip to z order here, to account for our right-handed coordinate system.
	// Unfortunately this means, we need to again flip the z coordinate of our texture coordinates in the shader.
	switch (cubemapIndex)
	{
		case 0: this->rotation = createQuaternionFromAxisAngle(vec3::up, DirectX::XMConvertToRadians(-90.f)); break;	// +X.
		case 1: this->rotation = createQuaternionFromAxisAngle(vec3::up, DirectX::XMConvertToRadians(90.f)); break;		// -X.
		case 2: this->rotation = createQuaternionFromAxisAngle(vec3::right, DirectX::XMConvertToRadians(90.f)); break;	// +Y.
		case 3: this->rotation = createQuaternionFromAxisAngle(vec3::right, DirectX::XMConvertToRadians(-90.f)); break; // -Y.
		case 4: this->rotation = quat::identity; break; 																// -Z.
		case 5: this->rotation = createQuaternionFromAxisAngle(vec3::up, DirectX::XMConvertToRadians(180.f)); break;	// +Z.
		default: assert(false);
	}

	this->fovY = DirectX::XMConvertToRadians(90.f);
	this->nearPlane = 0.1f;
	this->farPlane = 1000.f;

	updateMatrices(1, 1);
}

bool camera_frustum_planes::cullWorldSpaceAABB(const bounding_box& aabb) const
{
	for (uint32 i = 0; i < 6; ++i)
	{
		vec4 plane = planes[i];
		vec4 vertex(
			(plane.x < 0.f) ? aabb.min.x : aabb.max.x,
			(plane.y < 0.f) ? aabb.min.y : aabb.max.y,
			(plane.z < 0.f) ? aabb.min.z : aabb.max.z,
			1.f
		);
		if (dot4(plane, vertex) < 0.f)
		{
			return true;
		}
	}
	return false;
}

bool camera_frustum_planes::cullModelSpaceAABB(const bounding_box& aabb, const mat4& transform) const
{
	comp_mat m = transform;
	comp_vec worldSpaceCorners[] =
	{
		m * comp_vec(aabb.min.x, aabb.min.y, aabb.min.z, 1.f),
		m * comp_vec(aabb.max.x, aabb.min.y, aabb.min.z, 1.f),
		m * comp_vec(aabb.min.x, aabb.max.y, aabb.min.z, 1.f),
		m * comp_vec(aabb.max.x, aabb.max.y, aabb.min.z, 1.f),
		m * comp_vec(aabb.min.x, aabb.min.y, aabb.max.z, 1.f),
		m * comp_vec(aabb.max.x, aabb.min.y, aabb.max.z, 1.f),
		m * comp_vec(aabb.min.x, aabb.max.y, aabb.max.z, 1.f),
		m * comp_vec(aabb.max.x, aabb.max.y, aabb.max.z, 1.f),
	};

	for (uint32 i = 0; i < 6; ++i)
	{
		vec4 plane = planes[i];
		
		bool inside = false;

		for (uint32 j = 0; j < 8; ++j)
		{
			if (dot4(plane, worldSpaceCorners[j]) > 0.f)
			{
				inside = true;
				break;
			}
		}

		if (!inside)
		{
			return true;
		}
	}

	return false;
}
