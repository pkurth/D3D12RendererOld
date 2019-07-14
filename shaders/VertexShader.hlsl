struct ModelViewProjection
{
	matrix M;
	matrix V;
	matrix P;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);


struct vs_input
{
	float3 Position : POSITION;
	float2 UV		: TEXCOORDS;
	float3 Normal   : NORMAL;
};

struct vs_output
{
	float2 UV		: TEXCOORDS;
	float3 Normal   : NORMAL;
	float4 Position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	matrix MVP = mul(mul(ModelViewProjectionCB.P, ModelViewProjectionCB.V), ModelViewProjectionCB.M);
	OUT.Position = mul(MVP, float4(IN.Position, 1.f));
	OUT.Normal = mul(ModelViewProjectionCB.M, float4(IN.Normal, 0.f)).xyz;
	OUT.UV = IN.UV;

	return OUT;
}

