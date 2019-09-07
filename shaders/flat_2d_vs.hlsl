struct dim_cb
{
	float2 invScreenDim;
};

ConstantBuffer<dim_cb> dim : register(b0);

struct vs_input
{
	float2 position : POSITION;
	float4 color	: COLOR;
};

struct vs_output
{
	float4 color	: COLOR;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.color = IN.color;
	OUT.position = float4(2.f * dim.invScreenDim * IN.position - float2(1.f, 1.f), 0.f, 1.f);
	OUT.position.y = -OUT.position.y;
	return OUT;
}
