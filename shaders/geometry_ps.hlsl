struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
};

SamplerState texSampler	: register(s0);
Texture2D<float4> tex	: register(t0);

float4 main(ps_input IN) : SV_Target
{
	float4 color = tex.Sample(texSampler, IN.uv);
	if (color.a < 0.01f)
	{
		discard;
	}
	return color;
}
