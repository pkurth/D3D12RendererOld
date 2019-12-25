#include "camera.hlsli"
#include "pbr.hlsli"

#define BLOCK_SIZE 16

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer light_culling_cb : register(b0)
{
	uint screenWidth;
	uint screenHeight;
	uint numLights;
};

Texture2D<float> depthTexture		: register(t0);
StructuredBuffer<point_light> lights : register(t1);

ConstantBuffer<camera_cb> camera : register(b1);

RWStructuredBuffer<uint> lightIndexCounter : register(u0);
RWTexture2D<uint2> lightGrid : register(u1);
RWStructuredBuffer<uint> lightIndices : register(u2);


groupshared uint tileMinDepth;
groupshared uint tileMaxDepth;
groupshared uint tileLightCount;
groupshared uint tileLightIndices[1024];

groupshared uint lightIndexStartOffset;

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	uint2 texCoord = IN.dispatchThreadID.xy;

	float depth = depthTexture.Load(uint3(texCoord, 0));

	if (IN.groupIndex == 0)
	{
		tileMinDepth = 0xffffffff;
		tileMaxDepth = 0;
		tileLightCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();
	
	InterlockedMin(tileMinDepth, asuint(depth));
	InterlockedMax(tileMaxDepth, asuint(depth));

	GroupMemoryBarrierWithGroupSync();

	float minDepth = asfloat(tileMinDepth);
	float maxDepth = asfloat(tileMaxDepth);

	float minZ = depthBufferDepthToLinearWorldDepthEyeToFarPlane(minDepth, camera.projectionParams);
	float maxZ = depthBufferDepthToLinearWorldDepthEyeToFarPlane(maxDepth, camera.projectionParams);

	float2 tileScale = float2(screenWidth, screenHeight) * float2(1.f / (2.f * BLOCK_SIZE), 1.f / (2.f * BLOCK_SIZE));
	float2 tileBias = tileScale - float2(IN.groupID.xy);

	float4 c1 = float4(-camera.p[0][0] * tileScale.x, 0.f, tileBias.x, 0.f);
	float4 c2 = float4(0.0, -camera.p[1][1] * tileScale.y, tileBias.y, 0.f);
	const float4 c4 = float4(0.f, 0.f, -1.f, 0.f);

	float4 frustumPlanes[6];
	frustumPlanes[0] = c4 - c1;
	frustumPlanes[1] = c4 + c1;
	frustumPlanes[2] = c4 - c2;
	frustumPlanes[3] = c4 + c2;
	frustumPlanes[4] = float4(0.f, 0.f, -1.f, -minZ);
	frustumPlanes[5] = float4(0.f, 0.f, 1.f, maxZ);

	for (uint p = 0; p < 4; ++p)
	{
		frustumPlanes[p] /= length(frustumPlanes[p].xyz);
	}

	for (uint lightIndex = IN.groupIndex; lightIndex < numLights; lightIndex += BLOCK_SIZE * BLOCK_SIZE)
	{
		point_light light = lights[lightIndex];
		float attenuationRadius = light.worldSpacePositionAndRadius.w;

		float4 viewPos = mul(camera.v, float4(light.worldSpacePositionAndRadius.xyz, 1.f));

		bool inFrustum = true;
		for (uint i = 0; i < 6; ++i)
		{
			float d = dot(frustumPlanes[i], viewPos);
			inFrustum = inFrustum && (d >= -attenuationRadius);
		}

		if (inFrustum)
		{
			uint index; // Index into the visible lights array.
			InterlockedAdd(tileLightCount, 1, index);
			if (index < 1024)
			{
				tileLightIndices[index] = lightIndex;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		InterlockedAdd(lightIndexCounter[0], tileLightCount, lightIndexStartOffset);
		lightGrid[IN.groupID.xy] = uint2(lightIndexStartOffset, tileLightCount);
	}

	GroupMemoryBarrierWithGroupSync();

	for (uint i = IN.groupIndex; i < tileLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
	{
		lightIndices[lightIndexStartOffset + i] = tileLightIndices[i];
	}
}
