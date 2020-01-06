
cbuffer brush_cb : register(b1)
{
	float3 brushPosition;
	float brushRadius;
	float brushHardness;
	float brushStrength;
	uint channel;
};

struct ps_input
{
	float3 worldPosition	: POSITION;
};


float4 main(ps_input IN) : SV_TARGET
{
	float strength = pow(1.f - saturate(length(IN.worldPosition - brushPosition) / brushRadius), brushHardness) * brushStrength;

	float result[] = { 0.f, 0.f, 0.f, 0.f };
	result[channel] = strength;
	return float4(result[0], result[1], result[2], result[3]);
}
