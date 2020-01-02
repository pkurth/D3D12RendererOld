#include "material.hlsli"


struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

struct placement_point
{
	float4 position;
	float4 normal;
};

struct placement_mesh
{
	uint numTriangles;
	uint firstTriangle;
	uint baseVertex;
	uint textureID_usageFlags;
};

cbuffer placement_cb : register(b0)
{
	placement_mesh mesh0;
	placement_mesh mesh1;
	placement_mesh mesh2;
	placement_mesh mesh3;
	uint numMeshes;
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

StructuredBuffer<placement_point> placementPoints : register(t0);
StructuredBuffer<uint> numPlacementPoints		  : register(t1);

RWStructuredBuffer<indirect_command> outCommands : register(u0);
RWStructuredBuffer<indirect_depth_only_command> outDepthOnlyCommands : register(u1);

static const float4x4 identity =
{
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 }
};

#define BLOCK_SIZE 512

static void placeGeometry(float4x4 modelMatrix, placement_mesh mesh, uint outIndex)
{
	indirect_command result;
	result.modelMatrix = modelMatrix;

	result.material.albedoTint = float4(1.f, 1.f, 1.f, 1.f);
	result.material.textureID_usageFlags = mesh.textureID_usageFlags;
	result.material.roughnessOverride = 1.f;
	result.material.metallicOverride = 0.f;

	result.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
	result.drawArguments.InstanceCount = 1;
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

[numthreads(BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint numPoints = numPlacementPoints[0];
	if (IN.dispatchThreadID.x >= numPoints / numMeshes)
	{
		return;
	}

	float3 position = placementPoints[IN.dispatchThreadID.x].position.xyz;
	float3 normal = placementPoints[IN.dispatchThreadID.x].normal.xyz;

	float4x4 modelMatrix = identity;
	modelMatrix._m03 = position.x;
	modelMatrix._m13 = position.y;
	modelMatrix._m23 = position.z;

	placeGeometry(modelMatrix, mesh0, numMeshes * IN.dispatchThreadID.x + 0);
	if (numMeshes > 1)
	{
		placeGeometry(modelMatrix, mesh1, numMeshes * IN.dispatchThreadID.x + 1);
	}
	if (numMeshes > 2)
	{
		placeGeometry(modelMatrix, mesh2, numMeshes * IN.dispatchThreadID.x + 2);
	}
	if (numMeshes > 3)
	{
		placeGeometry(modelMatrix, mesh3, numMeshes * IN.dispatchThreadID.x + 3);
	}
}

