#ifndef CAMERA_H
#define CAMERA_H

struct camera_cb
{
	float4x4 vp;
	float4x4 v;
	float4x4 p;
	float4x4 invVP;
	float4x4 invV;
	float4x4 invP;
	float4 position;
	float4 forward;
	float4 projectionParams; // nearPlane, farPlane, farPlane / nearPlane, 1 - farPlane / nearPlane
};

static float3 restoreViewSpacePosition(float4x4 invProj, float2 uv, float depth)
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	float3 ndc = float3(uv * 2.f - float2(1.f, 1.f), depth);
	float4 homPosition = mul(invProj, float4(ndc, 1.f));
	float3 position = homPosition.xyz / homPosition.w;
	return position;
}

static float3 restoreWorldSpacePosition(float4x4 invViewProj, float2 uv, float depth)
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	float3 ndc = float3(uv * 2.f - float2(1.f, 1.f), depth);
	float4 homPosition = mul(invViewProj, float4(ndc, 1.f));
	float3 position = homPosition.xyz / homPosition.w;
	return position;
}

static float3 restoreViewDirection(float4x4 invProj, float2 uv)
{
	return restoreViewSpacePosition(invProj, uv, 1.f);
}

static float3 restoreWorldDirection(float4x4 invViewProj, float2 uv, float3 cameraPos, float farPlane)
{
	float3 direction = restoreWorldSpacePosition(invViewProj, uv, 1.f) - cameraPos; // At this point, the result should be on a plane 'farPlane' units away from the camera.
	direction /= farPlane;
	return direction;
}

static float depthBufferDepthToLinearNormalizedDepthEyeToFarPlane(float depthBufferDepth, float4 projectionParams)
{
	const float c1 = projectionParams.z;
	const float c0 = projectionParams.w;
	return 1.f / (c0 * depthBufferDepth + c1);
}

static float depthBufferDepthToLinearWorldDepthEyeToFarPlane(float depthBufferDepth, float4 projectionParams)
{
	return depthBufferDepthToLinearNormalizedDepthEyeToFarPlane(depthBufferDepth, projectionParams) * projectionParams.y;
}

#endif