
cbuffer unlit_cb : register(b0)
{
	float4x4 vp;
};

struct vs_input
{
	float3 position : POSITION;
	float2 uv : TEXCOORDS;
	float3 instancePosition : INSTANCEPOSITION;
	float4 color : INSTANCECOLOR;
	float2 instanceUV0 : INSTANCETEXCOORDS0;
	float2 instanceUV1 : INSTANCETEXCOORDS1;
};

struct vs_output
{
	float4 color	: COLOR;
	float2 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.color = IN.color;
	OUT.uv = float2(lerp(IN.instanceUV0.x, IN.instanceUV1.x, IN.uv.x), lerp(IN.instanceUV0.y, IN.instanceUV1.y, IN.uv.y));
	OUT.position = mul(vp, float4(IN.instancePosition + IN.position, 1.f));
	return OUT;
}