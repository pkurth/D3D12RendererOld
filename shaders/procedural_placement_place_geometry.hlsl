#include "material.hlsli"
#include "random.hlsli"
#include "camera.hlsli"

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

static const float pi = 3.141592653589793238462643383279f;

struct placement_point
{
	float4 position;
	float3 normal;
	uint id;
};

struct submesh_info
{
	float4 aabbMin;
	float4 aabbMax;
	uint numTriangles;
	uint firstTriangle;
	uint baseVertex;
	uint textureID_usageFlags;
};

struct placement_mesh
{
	uint firstSubmesh;
	uint numSubmeshes;
};

cbuffer camera_frustum_cb : register(b0)
{
	camera_frustum_planes cameraFrustum;
};

struct D3D12_DRAW_INDEXED_ARGUMENTS
{
	uint IndexCountPerInstance;
	uint InstanceCount;
	uint StartIndexLocation;
	int BaseVertexLocation;
	uint StartInstanceLocation;
};

struct indirect_command
{
	float4x4 modelMatrix;
	material_cb material;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct indirect_depth_only_command
{
	float4x4 modelMatrix;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	uint padding[3];
};

StructuredBuffer<placement_point> placementPoints	: register(t0);
StructuredBuffer<placement_mesh> meshes				: register(t1);
StructuredBuffer<submesh_info> submeshes			: register(t2);

RWStructuredBuffer<indirect_command> outCommands	: register(u0);
RWStructuredBuffer<indirect_depth_only_command> outDepthOnlyCommands : register(u1);
RWStructuredBuffer<uint> pointCounter				: register(u2);


#define BLOCK_SIZE 512


static void placeGeometry(float4x4 modelMatrix, submesh_info mesh, float4 color, uint outIndex)
{
	bool cull = cullModelSpaceAABB(cameraFrustum, mesh.aabbMin, mesh.aabbMax, modelMatrix);

	indirect_command result;
	result.modelMatrix = modelMatrix;

	result.material.albedoTint = color;
	result.material.textureID_usageFlags = mesh.textureID_usageFlags;
	result.material.roughnessOverride = 1.f;
	result.material.metallicOverride = 0.f;

	result.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
	result.drawArguments.InstanceCount = cull ? 0 : 1;
	result.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
	result.drawArguments.BaseVertexLocation = mesh.baseVertex;
	result.drawArguments.StartInstanceLocation = 0;


	indirect_depth_only_command depthOnlyResult;
	depthOnlyResult.modelMatrix = result.modelMatrix;
	depthOnlyResult.drawArguments = result.drawArguments;
	depthOnlyResult.padding[0] = depthOnlyResult.padding[1] = depthOnlyResult.padding[2] = 0;

	outCommands[outIndex] = result;
	outDepthOnlyCommands[outIndex] = depthOnlyResult;
}

groupshared uint groupCount;
groupshared uint startOffset;

[numthreads(BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	uint numPlacementPoints = pointCounter[1];
	uint groupIndex = 0;

	placement_point placementPoint = placementPoints[IN.dispatchThreadID.x];

	uint firstSubmesh = 0;
	uint numSubmeshes = 0;

	if (IN.dispatchThreadID.x < numPlacementPoints)
	{
		placement_mesh mesh = meshes[placementPoint.id];
		firstSubmesh = mesh.firstSubmesh;
		numSubmeshes = mesh.numSubmeshes;
	}

	InterlockedAdd(groupCount, numSubmeshes, groupIndex);

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		InterlockedAdd(pointCounter[0], groupCount, startOffset);
	}

	GroupMemoryBarrierWithGroupSync();


	if (numSubmeshes > 0)
	{
		float3 position = placementPoints[IN.dispatchThreadID.x].position.xyz;
		float3 yAxis = placementPoints[IN.dispatchThreadID.x].normal.xyz;

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
			submesh_info mesh = submeshes[firstSubmesh + i];
			float4 color = firstSubmesh == 0 ? float4(1, 0, 0, 1) : float4(1, 1, 1, 1);
			placeGeometry(modelMatrix, mesh, color, startOffset + groupIndex + i);
		}
	}
}

