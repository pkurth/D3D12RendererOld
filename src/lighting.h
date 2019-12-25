#pragma once

#include "common.h"
#include "math.h"

#include "camera.h"
#include "texture.h"

#include "buffer.h"
#include "root_signature.h"
#include "render_target.h"
#include "command_list.h"


#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f

struct directional_light
{
	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 worldSpaceDirection;
	vec4 color;

	uint32 numShadowCascades = 3;
	uint32 shadowMapDimensions = 2048;
	float shadowMapCascadeDistancePower = 2.f;
	float cascadeBlendArea = 0.1f;

	void updateMatrices(const camera_frustum& cameraWorldSpaceFrustum)
	{
		comp_mat viewMatrix = createLookAt(vec3(0.f, 0.f, 0.f), worldSpaceDirection, vec3(0.f, 1.f, 0.f));

		comp_vec nearBottomLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.nearBottomLeft, 1.f);
		comp_vec nearBottomRight = viewMatrix * vec4(cameraWorldSpaceFrustum.nearBottomRight, 1.f);
		comp_vec nearTopLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.nearTopLeft, 1.f);
		comp_vec nearTopRight = viewMatrix * vec4(cameraWorldSpaceFrustum.nearTopRight, 1.f);
		comp_vec farBottomLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.farBottomLeft, 1.f);
		comp_vec farBottomRight = viewMatrix * vec4(cameraWorldSpaceFrustum.farBottomRight, 1.f);
		comp_vec farTopLeft = viewMatrix * vec4(cameraWorldSpaceFrustum.farTopLeft, 1.f);
		comp_vec farTopRight = viewMatrix * vec4(cameraWorldSpaceFrustum.farTopRight, 1.f);

		bounding_box bb = bounding_box::negativeInfinity();
		bb.grow(nearBottomLeft);
		bb.grow(nearBottomRight);
		bb.grow(nearTopLeft);
		bb.grow(nearTopRight);

		for (uint32 i = 0; i < numShadowCascades; ++i)
		{
			float distance = powf((float)(i + 1) / numShadowCascades, shadowMapCascadeDistancePower);

			bb.grow(lerp(nearBottomLeft, farBottomLeft, distance));
			bb.grow(lerp(nearBottomRight, farBottomRight, distance));
			bb.grow(lerp(nearTopLeft, farTopLeft, distance));
			bb.grow(lerp(nearTopRight, farTopRight, distance));

			comp_mat projMatrix = createOrthographicMatrix(bb.min.x, bb.max.x, bb.max.y, bb.min.y, -bb.max.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -bb.min.z);

			vp[i] = projMatrix * viewMatrix;
		}
	}
};

struct point_light
{
	vec4 worldSpacePositionAndRadius;
	vec4 color;
};

struct spherical_harmonics
{
	vec4 coefficients[9];
};

struct packed_spherical_harmonics
{
	uint32 coefficients[9]; // Each int is 11 bits red, 11 bits green, 10 bits blue.
};

struct light_probe_tetrahedron
{
	union
	{
		struct
		{
			int a, b, c, d;
		};
		int indices[4];
	};
	union
	{
		struct
		{
			int na, nb, nc, nd;
		};
		int neighbors[4];
	};

	mat4 matrix;
};


#define VISUALIZE_LIGHTPROBE_ROOTPARAM_CB		0
#define VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE	1
#define VISUALIZE_LIGHTPROBE_ROOTPARAM_SH		1
#define VISUALIZE_LIGHTPROBE_ROOTPARAM_SH_INDEX 2

struct light_probe_system
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget,
		const std::vector<vec4>& lightProbePositions);

	// Visualizations of single probe.
	void visualizeCubemap(dx_command_list* commandList, const render_camera& camera, vec3 position, dx_texture& cubemap, float uvzScale = 1.f);
	void visualizeSH(dx_command_list* commandList, const render_camera& camera, vec3 position, const spherical_harmonics& sh, float uvzScale = 1.f);

	// Visualize whole system.
	void visualizeLightProbes(dx_command_list* commandList, const render_camera& camera, bool showProbes, bool showTetrahedralMesh,
		uint32 highlightTetrahedron);

	vec4 calculateBarycentricCoordinates(const light_probe_tetrahedron& tet, vec3 position);
	spherical_harmonics getInterpolatedSphericalHarmonics(const light_probe_tetrahedron& tet, vec4 barycentric);
	spherical_harmonics getInterpolatedSphericalHarmonics(uint32 tetrahedronIndex, vec4 barycentric);
	uint32 getEnclosingTetrahedron(vec3 position, uint32 lastTetrahedron, vec4& barycentric);



	dx_mesh lightProbeMesh;

	std::vector<vec4> lightProbePositions;
	dx_structured_buffer lightProbePositionBuffer;

	std::vector<light_probe_tetrahedron> lightProbeTetrahedra;
	dx_structured_buffer lightProbeTetrahedraBuffer;

	std::vector<spherical_harmonics> sphericalHarmonics;
	dx_structured_buffer sphericalHarmonicsBuffer;
	dx_structured_buffer packedSphericalHarmonicsBuffer;


	dx_mesh tetrahedronMesh;
	ComPtr<ID3D12PipelineState> unlitPipeline;
	dx_root_signature unlitRootSignature;

	ComPtr<ID3D12PipelineState> visualizeCubemapPipeline;
	dx_root_signature visualizeCubemapRootSignature;

	ComPtr<ID3D12PipelineState> visualizeSHBufferPipeline;
	dx_root_signature visualizeSHBufferRootSignature;

	ComPtr<ID3D12PipelineState> visualizeSHDirectPipeline;
	dx_root_signature visualizeSHDirectRootSignature;
};

