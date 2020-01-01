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

struct light_attenuation
{
	float constant = 1.f;
	float linear;
	float quadratic;


	float getAttenuation(float distance)
	{
		return 1.f / (constant + linear * distance + quadratic * distance * distance);
	}

	float getMaxDistance(float lightMax)
	{
		return (-linear + std::sqrtf(linear * linear - 4.f * quadratic * (constant - (256.f / 5.f) * lightMax)))
			/ (2.f * quadratic);
	}
};

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
	
	void updateMatrices(const render_camera& camera);
};

struct spot_light
{
	mat4 vp;

	vec4 worldSpacePosition;
	vec4 worldSpaceDirection;
	vec4 color;

	light_attenuation attenuation;

	float innerAngle;
	float outerAngle;
	float innerCutoff; // cos(innerAngle).
	float outerCutoff; // cos(outerAngle).
	float texelSize;
	float bias;
	uint32 shadowMapDimensions = 2048;

	void updateMatrices();
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

