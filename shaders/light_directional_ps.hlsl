struct ps_input
{
	float2 uv	: TEXCOORDS;
};

SamplerState texSampler		: register(s0);
Texture2D<float4> albedos	: register(t0);
Texture2D<float4> normals	: register(t1);

static const float3 L = normalize(float3(-1.f, -1.f, -1.f));

float4 main(ps_input IN) : SV_TARGET
{
	float4 albedo = albedos.Sample(texSampler, IN.uv);
	float3 normal = normals.Sample(texSampler, IN.uv).xyz;

	float NdotL = dot(normal, L);

	float4 color = albedo * NdotL;

	return color;
}