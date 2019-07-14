struct model_view_projection_cb
{
	matrix m;
	matrix v;
	matrix p;
};

ConstantBuffer<model_view_projection_cb> modelViewProjectionCB : register(b0);


struct vs_input
{
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
};

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	matrix mvp = mul(mul(modelViewProjectionCB.p, modelViewProjectionCB.v), modelViewProjectionCB.m);
	OUT.position = mul(mvp, float4(IN.position, 1.f));
	OUT.normal = mul(modelViewProjectionCB.m, float4(IN.normal, 0.f)).xyz;
	OUT.uv = IN.uv;

	return OUT;
}

