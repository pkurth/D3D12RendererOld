
struct ps_input
{
	float4 color	: COLOR;
	float2 uv		: TEXCOORDS;
};

SamplerState texSampler		: register(s0);
Texture2D<float4> tex		: register(t0);

float4 main(ps_input IN) : SV_TARGET
{
	return IN.color * tex.Sample(texSampler, IN.uv);
}
