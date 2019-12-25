#ifndef LIGHT_PROBE_H
#define LIGHT_PROBE_H


struct spherical_harmonics
{
	float4 coefficients[9];
};

struct packed_spherical_harmonics
{
	uint coefficients[9]; // Each int is 11 bits red, 11 bits green, 10 bits blue.
};

static float4 sampleSphericalHarmonics(spherical_harmonics sh, float3 normal)
{
	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	float4 result =
		sh.coefficients[0] +
		sh.coefficients[1] * y +
		sh.coefficients[2] * z +
		sh.coefficients[3] * x +
		sh.coefficients[4] * x * y +
		sh.coefficients[5] * y * z +
		sh.coefficients[6] * (3.f * z * z - 1.f) +
		sh.coefficients[7] * z * x +
		sh.coefficients[8] * (x * x - y * y)
		;

	return max(result, (float4)0);
}

static float3 unpackColorR11G11B10(uint c)
{
	const uint rMax = (1 << 11) - 1;
	const uint gMax = (1 << 11) - 1;
	const uint bMax = (1 << 10) - 1;

	float b = c & bMax;
	c >>= 10;
	float g = c & gMax;
	c >>= 11;
	float r = c;

	return float3(r * 3.f / rMax, g * 3.f / gMax, b * 3.f / bMax);
}

static float4 sampleSphericalHarmonics(packed_spherical_harmonics sh, float3 normal)
{
	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	float3 result =
		unpackColorR11G11B10(sh.coefficients[0]) +
		unpackColorR11G11B10(sh.coefficients[1]) * y +
		unpackColorR11G11B10(sh.coefficients[2]) * z +
		unpackColorR11G11B10(sh.coefficients[3]) * x +
		unpackColorR11G11B10(sh.coefficients[4]) * x * y +
		unpackColorR11G11B10(sh.coefficients[5]) * y * z +
		unpackColorR11G11B10(sh.coefficients[6]) * (3.f * z * z - 1.f) +
		unpackColorR11G11B10(sh.coefficients[7]) * z * x +
		unpackColorR11G11B10(sh.coefficients[8]) * (x * x - y * y)
		;

	return max(float4(result, 1.f), (float4)0);
}

static float4 sampleInterpolatedSphericalHarmonics(StructuredBuffer<spherical_harmonics> sphericalHarmonics, int4 indices, float4 barycentric, float3 normal)
{
	return
		sampleSphericalHarmonics(sphericalHarmonics[indices.x], normal) * barycentric.x +
		sampleSphericalHarmonics(sphericalHarmonics[indices.y], normal) * barycentric.y +
		sampleSphericalHarmonics(sphericalHarmonics[indices.z], normal) * barycentric.z +
		sampleSphericalHarmonics(sphericalHarmonics[indices.w], normal) * barycentric.w;
}

static float4 sampleInterpolatedSphericalHarmonics(StructuredBuffer<packed_spherical_harmonics> sphericalHarmonics, int4 indices, float4 barycentric, float3 normal)
{
	return
		sampleSphericalHarmonics(sphericalHarmonics[indices.x], normal) * barycentric.x +
		sampleSphericalHarmonics(sphericalHarmonics[indices.y], normal) * barycentric.y +
		sampleSphericalHarmonics(sphericalHarmonics[indices.z], normal) * barycentric.z +
		sampleSphericalHarmonics(sphericalHarmonics[indices.w], normal) * barycentric.w;
}

static spherical_harmonics getInterpolatedSphericalHarmonics(StructuredBuffer<spherical_harmonics> sphericalHarmonics, int4 indices, float4 barycentric)
{
	spherical_harmonics a = sphericalHarmonics[indices.x];
	spherical_harmonics b = sphericalHarmonics[indices.y];
	spherical_harmonics c = sphericalHarmonics[indices.z];
	spherical_harmonics d = sphericalHarmonics[indices.w];

	spherical_harmonics result;
	[unroll]
	for (uint i = 0; i < 9; ++i)
	{
		result.coefficients[i] = barycentric.x * a.coefficients[i]
			+ barycentric.y * b.coefficients[i]
			+ barycentric.z * c.coefficients[i]
			+ barycentric.w * d.coefficients[i];
	}
	return result;
}

struct light_probe_tetrahedron
{
	int4 indices;
	int4 neighbors;
	float4x4 barycentricMatrix;
};

static float4 calculateBarycentricCoordinates(StructuredBuffer<float4> lightProbePositions, light_probe_tetrahedron tet, float3 position)
{
	float4 barycentric = mul(tet.barycentricMatrix, float4(position - lightProbePositions[tet.indices.w].xyz, 1.f));
	barycentric.w = 1.f - barycentric.x - barycentric.y - barycentric.z;
	return barycentric;
}

static uint getEnclosingTetrahedron(StructuredBuffer<float4> lightProbePositions, StructuredBuffer<light_probe_tetrahedron> lightProbeTetrahedra, 
	float3 position, uint lastTetrahedron, out float4 barycentric)
{
	barycentric = calculateBarycentricCoordinates(lightProbePositions, lightProbeTetrahedra[lastTetrahedron], position);

	const uint maxNumIterations = 4;

	uint iterations = 0;

	while (any(barycentric < 0) && iterations < maxNumIterations)
	{
		uint smallestIndex = 0;
		float smallest = barycentric.x;
		for (uint i = 1; i < 4; ++i)
		{
			if (barycentric[i] < smallest)
			{
				smallest = barycentric[i];
				smallestIndex = i;
			}
		}

		int neighbor = lightProbeTetrahedra[lastTetrahedron].neighbors[smallestIndex];

		if (neighbor != -1)
		{
			lastTetrahedron = (uint)neighbor;
			barycentric = calculateBarycentricCoordinates(lightProbePositions, lightProbeTetrahedra[lastTetrahedron], position);
		}
		else
		{
			break;
		}

		++iterations;
	}

	return lastTetrahedron;
}

#endif
