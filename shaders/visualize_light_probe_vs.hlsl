
struct mvp_cb
{
	matrix mvp;
};

ConstantBuffer<mvp_cb> mvpCB : register(b0);

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
	OUT.position = mul(mvpCB.mvp, float4(IN.position, 1.f));

	return OUT;
}