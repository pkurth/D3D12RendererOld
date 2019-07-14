struct view_projection_cb
{
	matrix vp;
};

ConstantBuffer<view_projection_cb> viewProjectionCB : register(b0);

struct vs_input
{
	float3 position : POSITION;
};

struct vs_output
{
	float3 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	OUT.uv = IN.position;
	OUT.position = mul(viewProjectionCB.vp, float4(IN.position, 1.f));

	return OUT;
}