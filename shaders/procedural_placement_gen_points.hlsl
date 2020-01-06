#include "random.hlsli"
#include "camera.hlsli"
#include "placement.hlsli"

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

#define BLOCK_SIZE 16

cbuffer placement_cb : register(b0)
{
	float4 cameraPosition;
	uint4 options;

	float2 tileCorner;
	float tileSize;
	uint numDensityMaps;
	float groundHeight;

	float uvScale;
	float uvOffset;
};

SamplerState densitySampler							: register(s0);
StructuredBuffer<float2> samplePoints				: register(t0);
Texture2D<float4> densityMap						: register(t1);
StructuredBuffer<placement_mesh> meshes				: register(t2);

RWStructuredBuffer<placement_point> placementPoints : register(u0);
RWStructuredBuffer<uint> pointCount					: register(u1);
RWStructuredBuffer<uint> submeshCount				: register(u2);

groupshared uint groupCount;
groupshared uint groupStartOffset;


[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	float2 samplePoint = samplePoints[IN.groupIndex];
	float2 uv = samplePoint * uvScale + IN.groupID.xy * uvOffset;
	
	uint index = 100;
	if (all(uv >= 0.f && uv <= 1.f))
	{
		float4 densities = densityMap.SampleLevel(densitySampler, uv, 0);
		float densitySum = dot(densities, (float4)1.f);

		densitySum = max(densitySum, 1.f); // Only normalize if we are above 1.
		densities /= densitySum;

		float4 unusedChannels = float4(numDensityMaps < 1, numDensityMaps < 2, numDensityMaps < 3, numDensityMaps < 4);
		densities += unusedChannels; // Initialize unused channels to high.

		densities.y += densities.x;
		densities.z += densities.y;
		densities.w += densities.z;

		float threshold = random(uv);

		float4 comparison = (float4)threshold > densities;
		index = (uint)dot(comparison, comparison);
	}

	uint valid = index < numDensityMaps;

	uint innerGroupIndex;
	InterlockedAdd(groupCount, valid, innerGroupIndex);

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		InterlockedAdd(pointCount[0], groupCount, groupStartOffset);
	}

	GroupMemoryBarrierWithGroupSync();

	if (valid)
	{
		float2 xz = uv * tileSize + tileCorner;
		float3 position = float3(xz.x, groundHeight, xz.y);

		uint option = options[index];
		uint meshCount = option & 0xFFFF;
		uint meshIndex = (uint)(random(xz) * meshCount);
		meshIndex = min(meshIndex, meshCount - 1);
		meshIndex = (option >> 16) + meshIndex;


		// Calculate LOD.
		float distance = length(position - cameraPosition.xyz);

		placement_mesh mesh = meshes[meshIndex];
		uint numLODs = mesh.numLODs;
		float4 comparison = (float4)distance > float4(mesh.lodDistances, 999999.f);
		uint lodIndex = (uint)dot(float4(numLODs > 0, numLODs > 1, numLODs > 2, numLODs > 3), comparison);

		lodIndex = min(lodIndex, numLODs - 1);

		placement_lod lod = mesh.lods[lodIndex];

		uint firstSubmesh = lod.firstSubmesh;
		uint numSubmeshes = lod.numSubmeshes;

		for (uint i = firstSubmesh; i < firstSubmesh + numSubmeshes; ++i)
		{
			InterlockedAdd(submeshCount[i], 1);
		}


		placement_point result;
		result.position = position;
		result.normal = float3(0.f, 1.f, 0.f);
		result.meshID = meshIndex;
		result.lod = lodIndex;
		placementPoints[groupStartOffset + innerGroupIndex] = result;
	}

}
