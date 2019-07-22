struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	float x = -1.f + float((IN.vertexID & 1) << 2);
	float y = -1.f + float((IN.vertexID & 2) << 1);
	OUT.position = float4(x, y, 0.f, 1.f);
	OUT.uv.x = x * 0.5f + 0.5f;
	OUT.uv.y = 1.f - (y * 0.5f + 0.5f);

	return OUT;
}
