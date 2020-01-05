

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer draw_cb : register(b0)
{
	float worldSpaceSize;
}

RWTexture2D<float> tex;

[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	float currentColor = tex[IN.dispatchThreadID.xy];
	currentColor *= 2.f;
	tex[IN.dispatchThreadID.xy] = currentColor;

}


