#include "random.hlsli"
#include "camera.hlsli"

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

#define BLOCK_SIZE 16

struct placement_point
{
	float4 position;
	float4 normal;
};

cbuffer placement_cb : register(b0)
{
	uint numGroupsX;
	uint numGroupsY;
	uint numMeshes;
	float time;
};

SamplerState densitySampler							: register(s0);
Texture2D<float> densityMap							: register(t0);

RWStructuredBuffer<placement_point> placementPoints : register(u0);
RWStructuredBuffer<uint> atomicCounter				: register(u1);

static const float4x4 ditherMatrix =
{
	{ 0, 8, 2, 10 },
	{ 12, 4, 14, 6 },
	{ 3, 11, 1, 9 },
	{ 15, 7, 13, 5 }
};

groupshared uint groupCount;
groupshared uint startOffset;

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	uint densityWidth, densityHeight, numMipLevels;
	densityMap.GetDimensions(0, densityWidth, densityHeight, numMipLevels);

	float2 uv = float2(IN.dispatchThreadID.xy) / float2(numGroupsX * BLOCK_SIZE - 1, numGroupsY * BLOCK_SIZE - 1);
	float density = densityMap.SampleLevel(densitySampler, uv, 0);

	uint ditherX = IN.dispatchThreadID.x % 4;
	uint ditherY = IN.dispatchThreadID.y % 4;

	float ditherThreshold = ditherMatrix[ditherX][ditherY] / 16.f;
	uint valid = (uint)(density > ditherThreshold);

	uint groupIndex;
	InterlockedAdd(groupCount, valid, groupIndex);

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		InterlockedAdd(atomicCounter[0], groupCount * numMeshes, startOffset);
	}

	GroupMemoryBarrierWithGroupSync();

	if (valid)
	{
		placement_point result;
		result.position = float4(IN.dispatchThreadID.x * 10.f, sin(time + random(float3(IN.dispatchThreadID))) + 80.f, IN.dispatchThreadID.y * 10.f, 1.f);
		result.normal = float4(0.f, 1.f, 0.f, 0.f);
		placementPoints[startOffset + groupIndex] = result;
	}

}
