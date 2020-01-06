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
#define PROCEDURAL_MIN_FOOTPRINT 1.5f


#define PROCEDURAL_PLACEMENT_ALLOW_SIMULTANEOUS_EDITING 1


struct placement_lod
{
	uint32 firstSubmesh;
	uint32 numSubmeshes;
};

struct placement_mesh
{
	placement_lod lods[4];
	vec3 lodDistances; // Last LOD has infinite distance.
	uint32 numLODs;
};

STRINGIFY_ENUM(placementLayerNames,
enum placement_layer_name
{
	placement_layer_grass_and_pebbles,	"Grass and pebbles",
	placement_layer_cubes_and_spheres,	"Cubes and spheres",

	placement_layer_count,				"Count",
};
)

struct placement_layer_description
{
	float objectFootprint; // Diameter of one object in world space.
	uint32 meshOffset;
	uint32 numMeshes;

	const char* objectNames[4];
};

struct placement_layer
{
	bool active = false;
	dx_texture densities; // Packed into 4 channels.
};

struct placement_tile
{
	// Multiples of tile size.
	int32 cornerX;
	int32 cornerZ;

	float groundHeight; // TODO: Replace this with heightmap.
	float maximumHeight;

	placement_layer layers[placement_layer_count];

	// Filled out by placement system.
	bounding_box aabb;


	ComPtr<ID3D12Device2> device;
	void allocateLayer(placement_layer_name layerName);
};

struct procedural_placement
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, 
		std::vector<submesh_info>& submeshes, 
		const placement_mesh& grassMesh,
		const placement_mesh& cubeMesh,
		const placement_mesh& sphereMesh
	);

	void generate(const render_camera& camera);

	void transitionAllTexturesToCommon(dx_command_list* commandList);
	
	uint32 numDrawCalls;

	// For this frame.
	dx_structured_buffer commandBuffer;
	dx_structured_buffer depthOnlyCommandBuffer;
	dx_vertex_buffer instanceBuffer;

	int32 numTilesX;
	int32 numTilesZ;
	std::vector<placement_tile> tiles;

	placement_layer_description layerDescriptions[placement_layer_count];

private:
	uint32 maxNumInstances;

	dx_structured_buffer instanceBufferInternal;

	float radiusInUVSpace;

	struct procedural_placement_render_resources
	{
		dx_structured_buffer commandBuffer;
		dx_structured_buffer depthOnlyCommandBuffer;
		dx_structured_buffer instanceBuffer;
	};


	dx_structured_buffer samplePointsBuffer;
	dx_structured_buffer placementPointsBuffer;
	dx_structured_buffer numPlacementPointsBuffer;
	dx_structured_buffer meshBuffer;
	dx_structured_buffer submeshBuffer;
	dx_structured_buffer submeshCountBuffer;
	dx_structured_buffer submeshOffsetBuffer;

	dx_root_signature clearBufferRootSignature;
	ComPtr<ID3D12PipelineState> clearBufferPipelineState;

	dx_root_signature prefixSumRootSignature;
	ComPtr<ID3D12PipelineState> prefixSumPipelineState;

	dx_root_signature generatePointsRootSignature;
	ComPtr<ID3D12PipelineState> generatePointsPipelineState;

	dx_root_signature placeGeometryRootSignature;
	ComPtr<ID3D12PipelineState> placeGeometryPipelineState;

	dx_root_signature createCommandsRootSignature;
	ComPtr<ID3D12PipelineState> createCommandsPipelineState;


	procedural_placement_render_resources renderResources[NUM_BUFFERED_FRAMES];
	uint32 currentRenderResources = 0;

	void clearBuffer(dx_command_list* commandList, dx_structured_buffer& buffer);
	uint32 generatePoints(dx_command_list* commandList, vec3 cameraPosition, const camera_frustum_planes& frustum);
	void computeSubmeshOffsets(dx_command_list* commandList);
	void placeGeometry(dx_command_list* commandList, const camera_frustum_planes& frustum, uint32 maxNumGeneratedPlacementPoints);
	void createCommands(dx_command_list* commandList);
};
