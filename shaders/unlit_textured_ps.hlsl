
struct ps_input
{
	float2 uv		: TEXCOORDS;
	float4 color	: COLOR;
};

SamplerState texSampler		: register(s0);
Texture2D<float4> tex		: register(t0);

float4 main(ps_input IN) : SV_TARGET
{
	return IN.color * tex.Sample(texSampler, IN.uv);
}
