
cbuffer unlit_cb : register(b0)
{
	float4x4 mvp;
	float4 color;
};

struct vs_input
{
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
};

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float4 color	: COLOR;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.uv = IN.uv;
	OUT.color = color;
	OUT.position = mul(mvp, float4(IN.position, 1.f));
	return OUT;
}