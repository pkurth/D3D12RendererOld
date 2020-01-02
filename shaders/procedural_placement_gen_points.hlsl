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
	uint numMeshes;
	float time;
};

SamplerState densitySampler							: register(s0);
Texture2D<float> densityMap							: register(t0);
StructuredBuffer<float4> poissonMatrix				: register(t1);

RWStructuredBuffer<placement_point> placementPoints : register(u0);
RWStructuredBuffer<uint> atomicCounter				: register(u1);


groupshared uint groupCount;
groupshared uint startOffset;

[numthreads(512, 1, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	float3 poisson = poissonMatrix[IN.dispatchThreadID.x].xyz;

	float2 uv = poisson.xy;
	float threshold = poisson.z;
	
	float density = densityMap.SampleLevel(densitySampler, uv, 0);

	uint valid = (uint)(density > threshold);

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
		result.position = float4(uv.x * 100.f, sin(time + random(IN.dispatchThreadID.x)) + 40.f, uv.y * 100.f, 1.f);
		result.normal = float4(0.f, 1.f, 0.f, 0.f);
		placementPoints[startOffset + groupIndex] = result;
	}

}
