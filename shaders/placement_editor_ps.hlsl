
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
	float2 uv				: TEXCOORDS;
};


SamplerState linearWrapSampler	: register(s0);
Texture2D<float> densityMap		: register(t0);


float4 main(ps_input IN) : SV_TARGET
{
	float highlightBrightness = pow(1.f - saturate(length(IN.worldPosition - brushPosition) / brushRadius), brushHardness) * brushStrength;
	
	float density = densityMap.Sample(linearWrapSampler, IN.uv);
	
	return lerp((float4)density, float4(1.f, 0.f, 0.f, 1.f), highlightBrightness);
}
