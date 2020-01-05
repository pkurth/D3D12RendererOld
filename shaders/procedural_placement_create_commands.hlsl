#include "placement.hlsli"
#include "material.hlsli"

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer create_commands_cb : register(b0)
{
	uint size;
}

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
	material_cb material;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct indirect_depth_only_command
{
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	uint padding[3];
};

StructuredBuffer<submesh_info> submeshes	: register(t0);
StructuredBuffer<uint> submeshCounts		: register(t1);
StructuredBuffer<uint> submeshOffsets		: register(t2);

RWStructuredBuffer<indirect_command> commands						: register(u0);
RWStructuredBuffer<indirect_depth_only_command> depthOnlyCommands	: register(u1);

[numthreads(512, 1, 1)]
void main(cs_input IN)
{
	if (IN.dispatchThreadID.x >= size)
	{
		return;
	}

	submesh_info mesh = submeshes[IN.dispatchThreadID.x];
	uint maxCount = submeshOffsets[IN.dispatchThreadID.x + 1] - submeshOffsets[IN.dispatchThreadID.x];
	uint count = submeshCounts[IN.dispatchThreadID.x];
	count = maxCount - count;

	indirect_command result;

	result.material.albedoTint = float4(1.f, 1.f, 1.f, 1.f);
	result.material.textureID_usageFlags = mesh.textureID_usageFlags;
	result.material.roughnessOverride = 1.f;
	result.material.metallicOverride = 0.f;

	result.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
	result.drawArguments.InstanceCount = count;
	result.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
	result.drawArguments.BaseVertexLocation = mesh.baseVertex;
	result.drawArguments.StartInstanceLocation = submeshOffsets[IN.dispatchThreadID.x];


	indirect_depth_only_command depthOnlyResult;
	depthOnlyResult.drawArguments = result.drawArguments;
	depthOnlyResult.padding[0] = depthOnlyResult.padding[1] = depthOnlyResult.padding[2] = 0;

	commands[IN.dispatchThreadID.x] = result;
	depthOnlyCommands[IN.dispatchThreadID.x] = depthOnlyResult;
}

