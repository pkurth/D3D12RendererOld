struct PixelShaderInput
{
	float2 UV		: TEXCOORDS;
	float3 Normal   : NORMAL;
};

SamplerState linearClampSampler : register(s0);
Texture2D<float4> tex			: register(t0);

float4 main(PixelShaderInput IN) : SV_Target
{
	//return tex.Sample(linearClampSampler, IN.UV) * IN.Color;
	return float4(IN.Normal, 1.f);
}
