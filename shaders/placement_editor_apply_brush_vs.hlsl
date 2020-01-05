
cbuffer editor_cb : register(b0)
{
	float4x4 m;
};

struct vs_input
{
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
};

struct vs_output
{
	float3 worldPosition	: POSITION;
	float4 position			: SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.worldPosition = mul(m, float4(IN.position, 1.f)).xyz;
	OUT.position = float4(IN.uv * 2.f - float2(1.f, 1.f), 0.f, 1.f);
	OUT.position.y = -OUT.position.y;
	return OUT;
}