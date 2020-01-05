
// Algorithm from https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda.
// Only works for arrays up to 512 elements large.

cbuffer clear_cb : register(b0)
{
	uint size;
}

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
}; 

groupshared uint temp[512];

StructuredBuffer<uint> input		: register(t0);
RWStructuredBuffer<uint> output		: register(u0);

#define NUM_BANKS 16 
#define LOG_NUM_BANKS 4 
#define CONFLICT_FREE_OFFSET(n) ((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS)) 

[numthreads(512, 1, 1)]
void main(cs_input IN)
{
	uint threadID = IN.groupIndex;
	int offset = 1;
	uint n = size;

	int ai = threadID;
	int bi = threadID + (n / 2);
	int bankOffsetA = CONFLICT_FREE_OFFSET(ai); 
	int bankOffsetB = CONFLICT_FREE_OFFSET(bi); 
	temp[ai + bankOffsetA] = input[ai];
	temp[bi + bankOffsetB] = input[bi];


	{
		for (uint d = n >> 1; d > 0; d >>= 1)
		{
			GroupMemoryBarrierWithGroupSync();
			if (threadID < d)
			{
				int ai = offset * (2 * threadID + 1) - 1;
				int bi = offset * (2 * threadID + 2) - 1;
				ai += CONFLICT_FREE_OFFSET(ai);
				bi += CONFLICT_FREE_OFFSET(bi);

				temp[bi] += temp[ai];
			}
			offset *= 2;
		}
	}

	if (threadID == 0) 
	{ 
		temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)] = 0; 
	}

	{
		for (uint d = 1; d < n; d *= 2)
		{
			offset >>= 1;
			GroupMemoryBarrierWithGroupSync();
			if (threadID < d)
			{
				int ai = offset * (2 * threadID + 1) - 1;
				int bi = offset * (2 * threadID + 2) - 1;
				ai += CONFLICT_FREE_OFFSET(ai);
				bi += CONFLICT_FREE_OFFSET(bi);

				uint t = temp[ai];
				temp[ai] = temp[bi];
				temp[bi] += t;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	output[ai] = temp[ai + bankOffsetA]; 
	output[bi] = temp[bi + bankOffsetB];

	GroupMemoryBarrierWithGroupSync();

	if (threadID == 0)
	{
		output[n] = output[n - 1] + input[n - 1];
	}
}

