#pragma once

#include "command_list.h"
#include "camera.h"
#include "platform.h"

#define PROCEDURAL_PLACEMENT_ROOTPARAM_CB		0
#define PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS	1

// Same value, because different shaders.
#define PROCEDURAL_PLACEMENT_ROOTPARAM_DENSITY	2 // Density map and poisson distribution.
#define PROCEDURAL_PLACEMENT_ROOTPARAM_COMMANDS 2


struct placement_gen_points_cb
{
	uint32 numMeshes;
	float time;
};

struct placement_mesh
{
	uint32 numTriangles;
	uint32 firstTriangle;
	uint32 baseVertex;
	uint32 textureID_usageFlags;
};

struct placement_place_geometry_cb
{
	placement_mesh mesh0;
	placement_mesh mesh1;
	placement_mesh mesh2;
	placement_mesh mesh3;
	uint32 numMeshes;
};

struct placement_point
{
	vec4 position;
	vec4 normal;
};

struct procedural_placement_render_resources
{
	dx_structured_buffer numDrawCallsBuffer;
	dx_structured_buffer commandBuffer;
	dx_structured_buffer depthOnlyCommandBuffer;
};

struct procedural_placement
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList);
	void generate(const render_camera& camera, dx_texture& densityMap, 
		placement_mesh* meshes, uint32 numMeshes, float dt);
	
	uint32 maxNumDrawCalls;

	dx_structured_buffer numDrawCallsBuffer;
	dx_structured_buffer commandBuffer;
	dx_structured_buffer depthOnlyCommandBuffer;


private:
	dx_structured_buffer poissonSampleBuffer;
	dx_structured_buffer placementPointBuffer;

	dx_root_signature clearCountRootSignature;
	ComPtr<ID3D12PipelineState> clearCountPipelineState;

	dx_root_signature generatePointsRootSignature;
	ComPtr<ID3D12PipelineState> generatePointsPipelineState;

	dx_root_signature placeGeometryRootSignature;
	ComPtr<ID3D12PipelineState> placeGeometryPipelineState;


	procedural_placement_render_resources renderResources[NUM_BUFFERED_FRAMES];
	uint32 currentRenderResources = 0;

	void clearCount(dx_command_list* commandList);
	void generatePoints(dx_command_list* commandList, dx_texture& densityMap, uint32 numMeshes, float dt);
	void placeGeometry(dx_command_list* commandList, placement_mesh* meshes, uint32 numMeshes);
};
