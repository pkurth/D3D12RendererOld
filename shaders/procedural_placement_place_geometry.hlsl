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

static const float pi = 3.141592653589793238462643383279f;


cbuffer camera_frustum_cb : register(b0)
{
	camera_frustum_planes cameraFrustum;
};

StructuredBuffer<placement_point> placementPoints	: register(t0);
StructuredBuffer<placement_mesh> meshes				: register(t1);
StructuredBuffer<placement_submesh> submeshes		: register(t2);
StructuredBuffer<uint> submeshOffsets				: register(t3);

RWStructuredBuffer<uint> pointCount					: register(u0);
RWStructuredBuffer<uint> submeshCounts				: register(u1);
RWStructuredBuffer<float4x4> instanceData			: register(u2);

#define BLOCK_SIZE 512


static void placeGeometry(float4x4 modelMatrix, placement_submesh mesh, uint submeshIndex)
{
	bool cull = cullModelSpaceAABB(cameraFrustum, mesh.aabbMin, mesh.aabbMax, modelMatrix);

	if (!cull)
	{
		uint offset = submeshOffsets[submeshIndex];
		uint maxCount = submeshOffsets[submeshIndex + 1] - offset;

		uint index;
		InterlockedAdd(submeshCounts[submeshIndex], -1, index);
		index = maxCount - index;

		instanceData[offset + index] = transpose(modelMatrix);
	}
}


[numthreads(BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint numPlacementPoints = pointCount[0];

	if (IN.dispatchThreadID.x >= numPlacementPoints)
	{
		return;
	}


	placement_point placementPoint = placementPoints[IN.dispatchThreadID.x];
	placement_lod lod = meshes[placementPoint.meshID].lods[placementPoint.lod];
	
	uint firstSubmesh = lod.firstSubmesh;
	uint numSubmeshes = lod.numSubmeshes;

	if (numSubmeshes > 0)
	{
		float3 position = placementPoint.position.xyz;
		float3 yAxis = placementPoint.normal.xyz;

		float3 xAxis = normalize(cross(yAxis, float3(0.f, 0.f, 1.f)));
		float3 zAxis = cross(xAxis, yAxis);
		
		float x, y;
		float rotation = random(position.xz) * pi * 2.f;
		float scale = random(position.xz) * 0.5f + 0.75f;
		sincos(rotation, y, x);
		
		xAxis = x * xAxis + y * zAxis;
		zAxis = cross(xAxis, yAxis);

		xAxis *= scale;
		yAxis *= scale;
		zAxis *= scale;

		float4x4 modelMatrix =
		{
			{ xAxis.x, yAxis.x, zAxis.x, position.x },
			{ xAxis.y, yAxis.y, zAxis.y, position.y },
			{ xAxis.z, yAxis.z, zAxis.z, position.z },
			{ 0, 0, 0, 1 }
		};

		for (uint i = 0; i < numSubmeshes; ++i)
		{
			uint submeshIndex = firstSubmesh + i;
			placement_submesh mesh = submeshes[submeshIndex];
			float4 color = firstSubmesh == 0 ? float4(1, 0, 0, 1) : float4(1, 1, 1, 1);
			placeGeometry(modelMatrix, mesh, submeshIndex);
		}
	}
}

