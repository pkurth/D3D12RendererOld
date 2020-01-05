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

StructuredBuffer<uint> submeshCounts		: register(t0);
StructuredBuffer<uint> submeshOffsets		: register(t1);

RWStructuredBuffer<indirect_command> commands						: register(u0);
RWStructuredBuffer<indirect_depth_only_command> depthOnlyCommands	: register(u1);

[numthreads(512, 1, 1)]
void main(cs_input IN)
{
	uint threadID = IN.dispatchThreadID.x;

	if (threadID >= size)
	{
		return;
	}

	uint offset = submeshOffsets[threadID];
	uint maxCount = submeshOffsets[threadID + 1] - offset;
	uint count = submeshCounts[threadID];
	count = maxCount - count;


	commands[threadID].drawArguments.InstanceCount = count;
	commands[threadID].drawArguments.StartInstanceLocation = offset;

	depthOnlyCommands[threadID].drawArguments.InstanceCount = count;
	depthOnlyCommands[threadID].drawArguments.StartInstanceLocation = offset;
}

