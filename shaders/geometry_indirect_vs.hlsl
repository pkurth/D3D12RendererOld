#include "camera.hlsli"

struct model_cb
{
	float4x4 m;
};

ConstantBuffer<camera_cb> camera : register(b0);
ConstantBuffer<model_cb> model : register(b1);


struct vs_input
{
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
	float3 tangent  : TANGENT;
	uint lightProbeTetrahedron : LIGHTPROBE_TETRAHEDRON;
};

struct vs_output
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;
	nointerpolation uint lightProbeTetrahedron : LIGHTPROBE_TETRAHEDRON;
	float4 position			: SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	matrix mvp = mul(camera.vp, model.m);
	OUT.position = mul(mvp, float4(IN.position, 1.f));
	OUT.worldPosition = (mul(model.m, float4(IN.position, 1.f))).xyz;
	OUT.uv = IN.uv;

	float3 normal = normalize(mul(model.m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(model.m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent)); 
	OUT.tbn = float3x3(tangent, bitangent, normal);
	OUT.lightProbeTetrahedron = IN.lightProbeTetrahedron;
	return OUT;
}

