struct ps_input
{
	float2 uv	 : TEXCOORDS;
	float4 color : COLOR;
};

SamplerState texSampler		: register(s0);
Texture2D<float4> tex		: register(t0);

static const float edgeWidth = 0.1f;
static const float onEdge = 0.5f - edgeWidth * 0.5f;

static const float2 dropShadowOffset = float2(0.002f, 0.002f);
static const float3 dropShadowColor = float3(0.f, 0.f, 0.f);
static const float dropShadowOnEdge = onEdge + 0.15f;

float4 main(ps_input IN) : SV_TARGET
{
	float distance = 1.f - tex.Sample(texSampler, IN.uv).r;
	float alpha = 1.f - smoothstep(onEdge, onEdge + edgeWidth, distance);

	float distance2 = 1.f - tex.Sample(texSampler, IN.uv - dropShadowOffset).r;
	float outlineAlpha = 1.f - smoothstep(dropShadowOnEdge, dropShadowOnEdge + edgeWidth, distance2);

	float overallAlpha = alpha + (1.f - alpha) * outlineAlpha;
	float3 overallColor = lerp(dropShadowColor, IN.color.rgb, alpha / overallAlpha);

	overallAlpha *= IN.color.a;

	return float4(overallColor, overallAlpha);
}
