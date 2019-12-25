// This shader is supposed to be called with only one group, as to avoid global atomic adds.
// This is because InterlockedAdd only supports integers. Within a thread group, we can simply reduce.

#include "pbr.hlsli"
#include "light_probe.hlsli"

struct cs_input
{
	uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
	uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
	uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
	uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer cubemap_to_sh_cb : register(b0)
{
	uint mipLevel;
	uint outIndex;
};

RWStructuredBuffer<spherical_harmonics> sh : register(u0);

TextureCube<float4> srcTexture : register(t0);
SamplerState linearRepeatSampler : register(s0);


// Transform from dispatch ID to cubemap face direction
static const float3x3 rotateUV[6] = {
	// +X
	float3x3(0,  0,  1,
			 0, -1,  0,
			 -1,  0,  0),
	// -X
    float3x3(0,  0, -1,
    		 0, -1,  0,
    		 1,  0,  0),
	// +Y
	float3x3(1,  0,  0,
	         0,  0,  1,
			 0,  1,  0),
	// -Y
	float3x3(1,  0,  0,
	    	 0,  0, -1,
			 0, -1,  0),
	// +Z
	float3x3(1,  0,  0,
			 0, -1,  0,
			 0,  0,  1),
	// -Z
	float3x3(-1,  0,  0,
		     0,  -1,  0,
			 0,   0, -1)
};

static float areaElement(float2 xy)
{
	return atan2(xy.x * xy.y, length(float3(xy, 1.f)));
}

static float texelSolidAngle(float2 xy, float size)
{
	float invRes = 1.f / size;

	float u = ((float)xy.x + 0.5f) * invRes;
	float v = ((float)xy.y + 0.5f) * invRes;

	u = 2.f * u - 1.f;
	v = 2.f * v - 1.f;

	return areaElement(float2(u - invRes, v - invRes)) - areaElement(float2(u - invRes, v + invRes)) 
		- areaElement(float2(u + invRes, v - invRes)) + areaElement(float2(u + invRes, v + invRes));
}

#define BLOCK_SIZE_X 4
#define BLOCK_SIZE_Y 4
#define BLOCK_SIZE_Z 6

#define DISPATCH_SIZE (BLOCK_SIZE_X * BLOCK_SIZE_Y * BLOCK_SIZE_Z)

groupshared float4 gs_p0[DISPATCH_SIZE];
groupshared float4 gs_p1[DISPATCH_SIZE];
groupshared float4 gs_p2[DISPATCH_SIZE];
groupshared float4 gs_p3[DISPATCH_SIZE];
groupshared float4 gs_p4[DISPATCH_SIZE];
groupshared float4 gs_p5[DISPATCH_SIZE];
groupshared float4 gs_p6[DISPATCH_SIZE];
groupshared float4 gs_p7[DISPATCH_SIZE];
groupshared float4 gs_p8[DISPATCH_SIZE];
groupshared float gs_totalWeight[DISPATCH_SIZE];

[numthreads(BLOCK_SIZE_X, BLOCK_SIZE_Y, BLOCK_SIZE_Z)]
void main(cs_input IN)
{
	uint srcWidth, srcHeight, levels;
	srcTexture.GetDimensions(mipLevel, srcWidth, srcHeight, levels);

	uint cubemapSize = srcWidth;

	const uint2 workSize = uint2(cubemapSize, cubemapSize) / uint2(BLOCK_SIZE_X, BLOCK_SIZE_Y);
	const uint2 startIndex = IN.groupThreadID.xy * workSize;
	uint2 endIndex = startIndex + workSize;

	if (endIndex.x > cubemapSize)
	{
		endIndex.x = cubemapSize;
	}
	if (endIndex.y > cubemapSize)
	{
		endIndex.y = cubemapSize;
	}

	float4 p0 = (float4)0;
	float4 p1 = (float4)0;
	float4 p2 = (float4)0;
	float4 p3 = (float4)0;
	float4 p4 = (float4)0;
	float4 p5 = (float4)0;
	float4 p6 = (float4)0;
	float4 p7 = (float4)0;
	float4 p8 = (float4)0;

	float totalWeight = 0.f;

	for (uint y = startIndex.y; y < endIndex.y; ++y)
	{
		for (uint x = startIndex.x; x < endIndex.x; ++x)
		{
			float2 xy = float2(x, y);
			float3 dir = float3(xy / float(cubemapSize) - 0.5f, 0.5f);
			dir = normalize(mul(rotateUV[IN.groupThreadID.z], dir));

			float weight = texelSolidAngle(xy, float(cubemapSize));

			float weight1 = weight * (4.f / 17.f);
			float weight2 = weight * (8.f / 17.f);
			float weight3 = weight * (15.f / 17.f);
			float weight4 = weight * (5.f / 68.f);
			float weight5 = weight * (15.f / 68.f);

			float4 color = srcTexture.SampleLevel(linearRepeatSampler, dir, mipLevel);

			p0 += color * weight1;
			p1 += color * weight2 * dir.y;
			p2 += color * weight2 * dir.z;
			p3 += color * weight2 * dir.x;
			p4 += color * weight3 * dir.x * dir.y;
			p5 += color * weight3 * dir.y * dir.z;
			p6 += color * weight4 * (3.f * dir.z * dir.z - 1.f);
			p7 += color * weight3 * dir.z * dir.x;
			p8 += color * weight5 * (dir.x * dir.x - dir.y * dir.y);

			totalWeight += weight;
		}
	}
	
	gs_p0[IN.groupIndex] = p0;
	gs_p1[IN.groupIndex] = p1;
	gs_p2[IN.groupIndex] = p2;
	gs_p3[IN.groupIndex] = p3;
	gs_p4[IN.groupIndex] = p4;
	gs_p5[IN.groupIndex] = p5;
	gs_p6[IN.groupIndex] = p6;
	gs_p7[IN.groupIndex] = p7;
	gs_p8[IN.groupIndex] = p8;

	gs_totalWeight[IN.groupIndex] = totalWeight * 3.f;

	GroupMemoryBarrierWithGroupSync();

	for (uint stride = DISPATCH_SIZE / 2; stride > 0; stride /= 2)
	{
		if (IN.groupIndex < stride)
		{
			uint stridedIndex = IN.groupIndex + stride;
			gs_p0[IN.groupIndex] += gs_p0[stridedIndex];
			gs_p1[IN.groupIndex] += gs_p1[stridedIndex];
			gs_p2[IN.groupIndex] += gs_p2[stridedIndex];
			gs_p3[IN.groupIndex] += gs_p3[stridedIndex];
			gs_p4[IN.groupIndex] += gs_p4[stridedIndex];
			gs_p5[IN.groupIndex] += gs_p5[stridedIndex];
			gs_p6[IN.groupIndex] += gs_p6[stridedIndex];
			gs_p7[IN.groupIndex] += gs_p7[stridedIndex];
			gs_p8[IN.groupIndex] += gs_p8[stridedIndex];

			gs_totalWeight[IN.groupIndex] += gs_totalWeight[stridedIndex];
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if (IN.groupIndex == 0)
	{
		float scale = 4.f * pi / gs_totalWeight[0];

		sh[outIndex].coefficients[0] = gs_p0[0] * scale;
		sh[outIndex].coefficients[1] = gs_p1[0] * scale;
		sh[outIndex].coefficients[2] = gs_p2[0] * scale;
		sh[outIndex].coefficients[3] = gs_p3[0] * scale;
		sh[outIndex].coefficients[4] = gs_p4[0] * scale;
		sh[outIndex].coefficients[5] = gs_p5[0] * scale;
		sh[outIndex].coefficients[6] = gs_p6[0] * scale;
		sh[outIndex].coefficients[7] = gs_p7[0] * scale;
		sh[outIndex].coefficients[8] = gs_p8[0] * scale;

		/*sh[0].coefficients[0] = float4(0.445560, 0.264096, 0.266335, 0.0);
		sh[0].coefficients[1] = float4(-0.311523, -0.084572, 0.080637, 0.0);
		sh[0].coefficients[2] = float4(0.086516, 0.022305, -0.045806, 0.0);
		sh[0].coefficients[3] = float4(-0.091761, 0.026771, 0.100462, 0.0);
		sh[0].coefficients[4] = float4(0.063233, 0.061217, 0.087974, 0.0);
		sh[0].coefficients[5] = float4(0.003792, -0.022282, -0.050458, 0.0);
		sh[0].coefficients[6] = float4(-0.024107, -0.010575, -0.005569, 0.0);
		sh[0].coefficients[7] = float4(0.021007, 0.044164, 0.061526, 0.0);
		sh[0].coefficients[8] = float4(-0.000832, 0.012470, 0.024354, 0.0);*/
	}
}
