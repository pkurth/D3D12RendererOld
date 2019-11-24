
struct visualize_cb
{
	matrix mvp;
	float uvzScale;
};

ConstantBuffer<visualize_cb> visualizeCB : register(b0);

struct vs_input
{
	float3 position : POSITION;
};

struct vs_output
{
	float3 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	OUT.uv = IN.position;
	OUT.uv.z *= visualizeCB.uvzScale;
	OUT.position = mul(visualizeCB.mvp, float4(IN.position, 1.f));

	return OUT;
}