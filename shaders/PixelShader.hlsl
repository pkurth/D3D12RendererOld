struct PixelShaderInput
{
	float4 Color    : COLOR;
	float2 UV		: TEXCOORDS;
};

SamplerState linearClampSampler : register(s0);
Texture2D<float4> tex			: register(t0);

float4 main(PixelShaderInput IN) : SV_Target
{
	return tex.Sample(linearClampSampler, IN.UV) * IN.Color;
}
