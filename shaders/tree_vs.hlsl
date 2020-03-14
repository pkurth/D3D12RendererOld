#include "camera.hlsli"

struct tree_skin_cb
{
	float4x4 matrices[2];
};

ConstantBuffer<camera_cb> camera : register(b0);
ConstantBuffer<tree_skin_cb> skin : register(b1);


struct vs_input
{
	// Vertex data.
	float3 position : POSITION;
	float2 uv		: TEXCOORDS;
	float3 normal   : NORMAL;
	float3 tangent  : TANGENT;
	uint lightProbeTetrahedron : LIGHTPROBE_TETRAHEDRON;
	uint4 skinIndices	: SKINNING_INDICES;
	float4 skinWeights	: SKINNING_WEIGHTS;
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

	float4x4 m =
		IN.skinWeights[0] * skin.matrices[IN.skinIndices[0]] +
		IN.skinWeights[1] * skin.matrices[IN.skinIndices[1]];

	float3 position = IN.position;
	//position.x += IN.skinWeights[0];

	float4x4 mvp = mul(camera.vp, m);
	OUT.position = mul(mvp, float4(position, 1.f));
	OUT.worldPosition = (mul(m, float4(position, 1.f))).xyz;
	OUT.uv = IN.uv;

	float3 normal = normalize(mul(m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent));
	OUT.tbn = float3x3(tangent, bitangent, normal);
	OUT.lightProbeTetrahedron = IN.lightProbeTetrahedron;
	return OUT;
}

