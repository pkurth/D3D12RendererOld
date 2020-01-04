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
	float3 normal;
	uint id;
};

cbuffer placement_cb : register(b0)
{
	float2 tileCorner;
	float tileSize;
	uint numDensityMaps;
	uint meshOffset;
	float groundHeight;

	float uvScale;
	float uvOffset;
};

SamplerState densitySampler							: register(s0);
StructuredBuffer<float4> poissonMatrix				: register(t0);
Texture2D<float> densityMaps[4]						: register(t1); // TODO: Pack these together?

RWStructuredBuffer<placement_point> placementPoints : register(u0);
RWStructuredBuffer<uint> pointCounter				: register(u1);


groupshared uint groupCount;
groupshared uint startOffset;


[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	float3 poisson = poissonMatrix[IN.groupIndex].xyz;

	float2 uv = poisson.xy * uvScale + IN.groupID.xy * uvOffset;
	
	uint index = 100;
	if (all(uv >= 0.f && uv <= 1.f))
	{
		float4 densities = (float4)4; // Initialize to high.
		float densitySum = 0;
		[unroll]
		for (uint i = 0; i < numDensityMaps; ++i)
		{
			float density = densityMaps[i].SampleLevel(densitySampler, uv, 0);
			densities[i] = density;
			densitySum += density;
		}

		densitySum = max(densitySum, 1.f); // Only normalize if we are above 1.
		densities /= densitySum;

		densities.y += densities.x;
		densities.z += densities.y;
		densities.w += densities.z;

		float threshold = random(uv);

		float4 comparison = (float4)threshold > densities;
		index = (uint)dot(comparison, comparison);
	}

	uint valid = index < numDensityMaps;

	uint groupIndex;
	InterlockedAdd(groupCount, valid, groupIndex);

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		InterlockedAdd(pointCounter[1], groupCount, startOffset);
	}

	GroupMemoryBarrierWithGroupSync();

	if (valid)
	{
		placement_point result;

		float2 xz = uv * tileSize + tileCorner;

		result.position = float4(xz.x, groundHeight, xz.y, 1.f);
		result.normal = float3(0.f, 1.f, 0.f);
		result.id = index + meshOffset;
		placementPoints[startOffset + groupIndex] = result;
	}

}
