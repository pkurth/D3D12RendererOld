
cbuffer brush_cb : register(b1)
{
	float3 brushPosition;
	float brushRadius;
	float brushHardness;
	float brushStrength;
};

struct ps_input
{
	float3 worldPosition	: POSITION;
};


float4 main(ps_input IN) : SV_TARGET
{
	float strength = pow(1.f - saturate(length(IN.worldPosition - brushPosition) / brushRadius), brushHardness) * brushStrength;
	return (float4)strength;
}
