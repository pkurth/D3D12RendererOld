

struct camera_cb
{
	matrix invVP;
};

static float3 restoreViewSpacePosition(float4x4 invProj, float2 uv, float depth)
{
	float3 ndc = float3(uv * 2.f - float2(1.f, 1.f), depth);
	float4 homPosition = mul(invProj, float4(ndc, 1.f));
	float3 position = homPosition.xyz / homPosition.w;
	return position;
}

static float3 restoreWorldSpacePosition(float4x4 invViewProj, float2 uv, float depth)
{
	float3 ndc = float3(uv * 2.f - float2(1.f, 1.f), depth);
	float4 homPosition = mul(invViewProj, float4(ndc, 1.f));
	float3 position = homPosition.xyz / homPosition.w;
	return position;
}

static float3 restoreViewDirection(float4x4 invProj, float2 uv)
{
	return restoreViewSpacePosition(invProj, uv, 1.f);
}
