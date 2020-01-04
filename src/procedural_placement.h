#pragma once

#include "command_list.h"
#include "camera.h"
#include "platform.h"
#include "math.h"

#define PROCEDURAL_PLACEMENT_ROOTPARAM_CB		0 // For point generation.
#define PROCEDURAL_PLACEMENT_ROOTPARAM_CAMERA	0 // For geometry placement.
#define PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS		1
#define PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS		2


#define PROCEDURAL_TILE_SIZE 100.f
#define PROCEDURAL_MIN_FOOTPRINT 4.f

struct placement_mesh
{
	uint32 firstSubmesh;
	uint32 numSubmeshes;
};

struct placement_tile
{
	// Multiples of tile size.
	uint32 cornerX;
	uint32 cornerZ;

	float groundHeight;
	float maximumHeight;

	float objectFootprint; // Diameter of one object in world space.
	uint32 numMeshes;

	placement_mesh meshes[4];
	dx_texture* densities[4];


	// Filled out by placement system.
	uint32 meshOffset;
};

struct procedural_placement
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, 
		std::vector<placement_tile>& tiles, std::vector<submesh_info>& subMeshes);

	void generate(const render_camera& camera);
	
	uint32 maxNumDrawCalls;

	// For this frame.
	dx_structured_buffer numDrawCallsBuffer;
	dx_structured_buffer commandBuffer;
	dx_structured_buffer depthOnlyCommandBuffer;


	std::vector<placement_tile> tiles;

private:


	float radiusInUVSpace;

	struct procedural_placement_render_resources
	{
		dx_structured_buffer numDrawCallsBuffer;
		dx_structured_buffer commandBuffer;
		dx_structured_buffer depthOnlyCommandBuffer;
	};


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
	uint32 generatePoints(dx_command_list* commandList, const camera_frustum_planes& frustum);
	void placeGeometry(dx_command_list* commandList, const camera_frustum_planes& frustum, uint32 maxNumGeneratedPlacementPoints);
};
