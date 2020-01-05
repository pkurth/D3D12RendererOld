
cbuffer editor_cb : register(b0)
{
	float4x4 m;
	float4x4 mvp;
};

struct vs_input
{
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
};

struct vs_output
{
	float3 worldPosition	: POSITION;
	float2 uv				: TEXCOORDS;
	float4 position			: SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.worldPosition = mul(m, float4(IN.position, 1.f)).xyz;
	OUT.position = mul(mvp, float4(IN.position, 1.f));
	OUT.uv = IN.uv;
	return OUT;
}