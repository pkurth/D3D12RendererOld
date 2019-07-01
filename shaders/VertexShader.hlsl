struct ModelViewProjection
{
	matrix M;
	matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexPosColor
{
	float3 Position : POSITION;
	float2 UV		: TEXCOORDS;
	float3 Normal   : NORMAL;
};

struct VertexShaderOutput
{
	float2 UV		: TEXCOORDS;
	float3 Normal   : NORMAL;
	float4 Position : SV_Position;
};

VertexShaderOutput main(VertexPosColor IN)
{
	VertexShaderOutput OUT;

	OUT.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.f));
	OUT.Normal = mul(ModelViewProjectionCB.M, float4(IN.Normal, 0.f)).xyz;
	OUT.UV = IN.UV;

	return OUT;
}

