#pragma once

#include "command_list.h"
#include "camera.h"
#include "platform.h"
#include "math.h"

#define PROCEDURAL_PLACEMENT_ROOTPARAM_CB		0
#define PROCEDURAL_PLACEMENT_ROOTPARAM_CAMERA	0
#define PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS		1
#define PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS		2


struct placement_gen_points_cb
{
	uint32 numDensityMaps;
	float time;
};

struct placement_mesh
{
	uint32 offset;
	uint32 count;
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
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, 
		std::vector<placement_mesh>& meshes, std::vector<submesh_info>& subMeshes);
	void generate(const render_camera& camera, dx_texture& densityMap0, dx_texture& densityMap1, float dt);
	
	uint32 maxNumDrawCalls;

	dx_structured_buffer numDrawCallsBuffer;
	dx_structured_buffer commandBuffer;
	dx_structured_buffer depthOnlyCommandBuffer;


private:
	D3D12_CPU_DESCRIPTOR_HANDLE defaultSRV;

	dx_structured_buffer poissonSampleBuffer;
	dx_structured_buffer placementPointBuffer;
	dx_structured_buffer meshBuffer;
	dx_structured_buffer submeshBuffer;

	dx_root_signature clearCountRootSignature;
	ComPtr<ID3D12PipelineState> clearCountPipelineState;

	dx_root_signature generatePointsRootSignature;
	ComPtr<ID3D12PipelineState> generatePointsPipelineState;

	dx_root_signature placeGeometryRootSignature;
	ComPtr<ID3D12PipelineState> placeGeometryPipelineState;


	procedural_placement_render_resources renderResources[NUM_BUFFERED_FRAMES];
	uint32 currentRenderResources = 0;

	void clearCount(dx_command_list* commandList);
	void generatePoints(dx_command_list* commandList, dx_texture& densityMap0, dx_texture& densityMap1, float dt);
	void placeGeometry(dx_command_list* commandList, const render_camera& camera);
};
