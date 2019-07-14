struct ps_input
{
	float2 UV		: TEXCOORDS;
	float3 Normal   : NORMAL;
};

SamplerState texSampler : register(s0);
Texture2D<float4> tex			: register(t0);

float4 main(ps_input IN) : SV_Target
{
	float4 color = tex.Sample(texSampler, IN.UV);
	if (color.a < 0.01f)
	{
		discard;
	}
	return color;
	//return float4(IN.Normal, 1.f);
}
