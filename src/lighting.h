#pragma once

#include "common.h"
#include "math.h"

#include "camera.h"
#include "texture.h"

#include "buffer.h"
#include "root_signature.h"
#include "render_target.h"
#include "command_list.h"
#include "debug_display.h"


#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f

#define LIGHT_PROBE_RESOLUTION 32


struct directional_light
{
	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];
	vec4 cascadeDistances;
	vec4 bias;

	vec4 worldSpaceDirection;
	vec4 color;

	uint32 numShadowCascades = 3;
	float blendArea;
	float texelSize;
	uint32 shadowMapDimensions = 2048;
	

	void updateMatrices(const render_camera& camera)
	{
		comp_mat viewMatrix = createLookAt(vec3(0.f, 0.f, 0.f), worldSpaceDirection, vec3(0.f, 1.f, 0.f));
		
		vec3 worldForward = camera.rotation * vec3(0.f, 0.f, -1.f);
		camera_frustum worldFrustum = camera.getWorldSpaceFrustum();

		comp_vec worldBottomLeft = worldFrustum.farBottomLeft - worldFrustum.nearBottomLeft;
		comp_vec worldBottomRight = worldFrustum.farBottomRight - worldFrustum.nearBottomRight;
		comp_vec worldTopLeft = worldFrustum.farTopLeft - worldFrustum.nearTopLeft;
		comp_vec worldTopRight = worldFrustum.farTopRight - worldFrustum.nearTopRight;

		worldBottomLeft /= dot3(worldBottomLeft, worldForward);
		worldBottomRight /= dot3(worldBottomRight, worldForward);
		worldTopLeft /= dot3(worldTopLeft, worldForward);
		worldTopRight /= dot3(worldTopRight, worldForward);

		comp_vec worldEye = vec4(camera.position, 1.f);
		comp_vec sunEye = viewMatrix * worldEye;

		bounding_box initialBB = bounding_box::negativeInfinity();
		initialBB.grow(sunEye);

		for (uint32 i = 0; i < numShadowCascades; ++i)
		{
			float distance = cascadeDistances.data[i];

			comp_vec sunBottomLeft = viewMatrix * (worldEye + distance * worldBottomLeft);
			comp_vec sunBottomRight = viewMatrix * (worldEye + distance * worldBottomRight);
			comp_vec sunTopLeft = viewMatrix * (worldEye + distance * worldTopLeft);
			comp_vec sunTopRight = viewMatrix * (worldEye + distance * worldTopRight);

			bounding_box bb = initialBB;
			bb.grow(sunBottomLeft);
			bb.grow(sunBottomRight);
			bb.grow(sunTopLeft);
			bb.grow(sunTopRight);

			bb.expand(2.f);

			comp_mat projMatrix = createOrthographicMatrix(bb.min.x, bb.max.x, bb.max.y, bb.min.y, -bb.max.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -bb.min.z);

			vp[i] = projMatrix * viewMatrix;
		}

		texelSize = 1.f / (float)shadowMapDimensions;
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
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget,
		const std::vector<vec4>& lightProbePositions, const std::vector<spherical_harmonics>& sphericalHarmonics);

	void setSphericalHarmonics(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const std::vector<spherical_harmonics>& sphericalHarmonics);

	// Visualizations of single probe.
	void visualizeLightProbeCubemaps(dx_command_list* commandList, const render_camera& camera, float uvzScale = 1.f);
	void visualizeCubemap(dx_command_list* commandList, const render_camera& camera, vec3 position, dx_texture& cubemap, float uvzScale = 1.f);
	void visualizeSH(dx_command_list* commandList, const render_camera& camera, vec3 position, const spherical_harmonics& sh, float uvzScale = 1.f);

	// Visualize whole system.
	void visualizeLightProbes(dx_command_list* commandList, const render_camera& camera, bool showProbes, bool showTetrahedralMesh,
		debug_display& debugDisplay);

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
	dx_structured_buffer packedSphericalHarmonicsBuffer;

	dx_structured_buffer tempSphericalHarmonicsBuffer;


	dx_mesh tetrahedronMesh;

	ComPtr<ID3D12PipelineState> visualizeCubemapPipeline;
	dx_root_signature visualizeCubemapRootSignature;

	ComPtr<ID3D12PipelineState> visualizeSHBufferPipeline;
	dx_root_signature visualizeSHBufferRootSignature;

	ComPtr<ID3D12PipelineState> visualizeSHDirectPipeline;
	dx_root_signature visualizeSHDirectRootSignature;


	// Light probe rendering.
	dx_render_target lightProbeRT;
	dx_texture lightProbeHDRTexture;
	dx_texture lightProbeDepthTexture;

};

