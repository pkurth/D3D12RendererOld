#include "camera.hlsli"


ConstantBuffer<camera_cb> camera : register(b0);


struct vs_input
{
	// Vertex data.
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
	float3 tangent  : TANGENT;
	uint lightProbeTetrahedron : LIGHTPROBE_TETRAHEDRON;


	// Instance data.
	float4 mRow0 : MODELMATRIX0;
	float4 mRow1 : MODELMATRIX1;
	float4 mRow2 : MODELMATRIX2;
	float4 mRow3 : MODELMATRIX3;
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

	float4x4 m = {
		IN.mRow0,
		IN.mRow1,
		IN.mRow2,
		IN.mRow3
	};

	float4x4 mvp = mul(camera.vp, m);
	OUT.position = mul(mvp, float4(IN.position, 1.f));
	OUT.worldPosition = (mul(m, float4(IN.position, 1.f))).xyz;
	OUT.uv = IN.uv;

	float3 normal = normalize(mul(m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent)); 
	OUT.tbn = float3x3(tangent, bitangent, normal);
	OUT.lightProbeTetrahedron = IN.lightProbeTetrahedron;
	return OUT;
}

