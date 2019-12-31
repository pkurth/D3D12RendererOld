
cbuffer unlit_cb : register(b0)
{
	float4x4 vp;
	float4 color;
};

struct vs_input
{
	float3 position : POSITION;
	float3 instancePosition : INSTANCEPOSITION;
};

struct vs_output
{
	float4 color	: COLOR;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.color = color;
	OUT.position = mul(vp, float4(IN.instancePosition + IN.position, 1.f));
	return OUT;
}