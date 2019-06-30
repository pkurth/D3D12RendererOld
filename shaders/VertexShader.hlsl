struct ModelViewProjection
{
	matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexPosColor
{
	float3 Position : POSITION;
	float3 Color    : COLOR;
	float2 UV		: TEXCOORDS;
};

struct VertexShaderOutput
{
	float4 Color    : COLOR;
	float2 UV		: TEXCOORDS;
	float4 Position : SV_Position;
};

VertexShaderOutput main(VertexPosColor IN)
{
	VertexShaderOutput OUT;

	OUT.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.f));
	OUT.Color = float4(IN.Color, 1.f);
	OUT.UV = IN.UV;

	return OUT;
}

