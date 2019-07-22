struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
};

struct ps_output
{
	float4 albedo	: SV_Target0;
	float4 emission : SV_Target1;
	float4 normal	: SV_Target2;
};

SamplerState texSampler	: register(s0);
Texture2D<float4> tex	: register(t0);

ps_output main(ps_input IN)
{
	ps_output OUT;

	float4 color = tex.Sample(texSampler, IN.uv);
	if (color.a < 0.01f)
	{
		discard;
	}

	OUT.albedo = color;
	OUT.emission = float4(0.f, 0.f, 0.f, 0.f);
	OUT.normal = float4(IN.normal, 0.f);

	return OUT;
}
