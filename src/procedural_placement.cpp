#include "pch.h"
#include "procedural_placement.h"
#include "error.h"
#include "graphics.h"
#include "indirect_drawing.h"
#include "command_queue.h"
#include "profiling.h"
#include "poisson_distribution.h"

#include <pix3.h>

struct placement_gen_points_cb
{
	vec4 cameraPosition;
	uint32 options[4];
	vec2 tileCorner;
	float tileSize;
	uint32 numDensityMaps;
	float groundHeight;

	float uvScale;
	float uvOffset;
};

struct placement_point
{
	vec4 position;
	vec4 normal;
};

struct placement_submesh
{
	vec4 aabbMin;
	vec4 aabbMax;
};

// Dither algorithms:
// https://www.shadertoy.com/view/XlyXWW

void procedural_placement::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList,
	const std::vector<submesh_info>& submeshes,
	const std::vector<placement_mesh>& grassMeshes,
	const placement_mesh& cubeMesh,
	const placement_mesh& sphereMesh,
	const placement_mesh& treeMesh)
{
	{
		ComPtr<ID3DBlob> shaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_clear_buffer.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 buffer(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsConstants(1, 0);
		rootParameters[1].InitAsDescriptorTable(1, &buffer);


		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		clearBufferRootSignature.initialize(device, rootSignatureDesc);

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS cs;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = clearBufferRootSignature.rootSignature.Get();
		pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&clearBufferPipelineState)));

		SET_NAME(clearBufferRootSignature.rootSignature, "Procedural Placement Clear Buffer Root Signature");
		SET_NAME(clearBufferPipelineState, "Procedural Placement Clear Buffer Pipeline");
	}

	{
		ComPtr<ID3DBlob> shaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_prefix_sum.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 inputBuffer(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_DESCRIPTOR_RANGE1 outputBuffer(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_CB].InitAsConstants(1, 0);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS].InitAsDescriptorTable(1, &inputBuffer);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS].InitAsDescriptorTable(1, &outputBuffer);


		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		prefixSumRootSignature.initialize(device, rootSignatureDesc);

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS cs;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = prefixSumRootSignature.rootSignature.Get();
		pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&prefixSumPipelineState)));

		SET_NAME(prefixSumRootSignature.rootSignature, "Procedural Placement Prefix Sum Root Signature");
		SET_NAME(prefixSumPipelineState, "Procedural Placement Prefix Sum Pipeline");
	}

	{
		ComPtr<ID3DBlob> shaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_gen_points.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // Sample positions, density map and meshes.
		CD3DX12_DESCRIPTOR_RANGE1 uavs(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Generated points, point count and submesh count.

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_CB].InitAsConstants(sizeof(placement_gen_points_cb) / sizeof(float), 0);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS].InitAsDescriptorTable(1, &uavs);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS].InitAsDescriptorTable(1, &srvs);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		generatePointsRootSignature.initialize(device, rootSignatureDesc);

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS cs;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = generatePointsRootSignature.rootSignature.Get();
		pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&generatePointsPipelineState)));

		SET_NAME(generatePointsRootSignature.rootSignature, "Procedural Placement Generate Points Root Signature");
		SET_NAME(generatePointsPipelineState, "Procedural Placement Generate Points Pipeline");
	}

	{
		ComPtr<ID3DBlob> shaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_place_geometry.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
		CD3DX12_DESCRIPTOR_RANGE1 uavs(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_CAMERA].InitAsConstants(sizeof(camera_frustum_planes) / sizeof(float), 0);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS].InitAsDescriptorTable(1, &uavs);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS].InitAsDescriptorTable(1, &srvs);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		placeGeometryRootSignature.initialize(device, rootSignatureDesc);

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS cs;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = placeGeometryRootSignature.rootSignature.Get();
		pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&placeGeometryPipelineState)));

		SET_NAME(placeGeometryRootSignature.rootSignature, "Procedural Placement Place Geometry Root Signature");
		SET_NAME(placeGeometryPipelineState, "Procedural Placement Place Geometry Pipeline");
	}

	{
		ComPtr<ID3DBlob> shaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_create_commands.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		CD3DX12_DESCRIPTOR_RANGE1 uavs(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_CB].InitAsConstants(1, 0);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS].InitAsDescriptorTable(1, &uavs);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS].InitAsDescriptorTable(1, &srvs);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		createCommandsRootSignature.initialize(device, rootSignatureDesc);

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS cs;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = createCommandsRootSignature.rootSignature.Get();
		pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&createCommandsPipelineState)));

		SET_NAME(createCommandsRootSignature.rootSignature, "Procedural Placement Create Commands Root Signature");
		SET_NAME(createCommandsPipelineState, "Procedural Placement Create Commands Pipeline");
	}

	numTilesX = 10;
	numTilesZ = 10;
	this->tiles.resize(numTilesX * numTilesZ);

	std::vector<placement_mesh> meshes;
	append(meshes, grassMeshes);
	meshes.push_back(cubeMesh);
	meshes.push_back(sphereMesh);
	meshes.push_back(treeMesh);

	uint32 cubeMeshOffset = (uint32)grassMeshes.size();
	uint32 sphereMeshOffset = cubeMeshOffset + 1;
	uint32 treeMeshOffset = sphereMeshOffset + 1;

	uint32 i = 0;
	for (int32 z = -numTilesZ / 2; z < numTilesZ / 2; ++z)
	{
		for (int32 x = -numTilesX / 2; x < numTilesX / 2; ++x)
		{
			placement_tile& tile = this->tiles[i];
			
			tile.cornerX = x;
			tile.cornerZ = z;

			tile.groundHeight = 0.f;
			tile.maximumHeight = 20.f;

			bounding_box tileBB = bounding_box::negativeInfinity();
			vec2 corner(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);
			tileBB.grow(vec3(corner.x, tile.groundHeight, corner.y));
			tileBB.grow(vec3(corner.x + PROCEDURAL_TILE_SIZE, tile.groundHeight + tile.maximumHeight, corner.y + PROCEDURAL_TILE_SIZE));
			tile.aabb = tileBB;

			tile.device = device;

			++i;
		}
	}

	layerDescriptions[placement_layer_grass_and_pebbles].numDensityMaps = 1;
	layerDescriptions[placement_layer_grass_and_pebbles].objectFootprint = 2.f;
	layerDescriptions[placement_layer_grass_and_pebbles].objectNames[0] = "Grass";
	layerDescriptions[placement_layer_grass_and_pebbles].options[0].offset = 0;
	layerDescriptions[placement_layer_grass_and_pebbles].options[0].count = (uint32)grassMeshes.size();

	layerDescriptions[placement_layer_cubes_and_spheres].numDensityMaps = 2;
	layerDescriptions[placement_layer_cubes_and_spheres].objectFootprint = 4.f;
	layerDescriptions[placement_layer_cubes_and_spheres].objectNames[0] = "Cubes";
	layerDescriptions[placement_layer_cubes_and_spheres].options[0].offset = cubeMeshOffset;
	layerDescriptions[placement_layer_cubes_and_spheres].options[0].count = 1;
	layerDescriptions[placement_layer_cubes_and_spheres].objectNames[1] = "Spheres";
	layerDescriptions[placement_layer_cubes_and_spheres].options[1].offset = sphereMeshOffset;
	layerDescriptions[placement_layer_cubes_and_spheres].options[1].count = 1;

	layerDescriptions[placement_layer_trees].numDensityMaps = 1;
	layerDescriptions[placement_layer_trees].objectFootprint = 6.f;
	layerDescriptions[placement_layer_trees].objectNames[0] = "Trees";
	layerDescriptions[placement_layer_trees].options[0].offset = treeMeshOffset;
	layerDescriptions[placement_layer_trees].options[0].count = 1;


	radiusInUVSpace = sqrt(1.f / (2.f * sqrt(3.f) * arraysize(POISSON_SAMPLES)));
	float diameterInUVSpace = radiusInUVSpace * 2.f;

	float diameterInWorldSpace = diameterInUVSpace * PROCEDURAL_TILE_SIZE;

	float scaling = diameterInWorldSpace / PROCEDURAL_MIN_FOOTPRINT;
	uint32 numGroupsPerDim = (uint32)ceil(scaling);
	uint32 maxNumObjectFactor = numGroupsPerDim * numGroupsPerDim;

	numDrawCalls = (uint32)submeshes.size();


	std::vector<indirect_command> commands(numDrawCalls);
	std::vector<indirect_depth_only_command> depthOnlyCommands(numDrawCalls);
	std::vector<placement_submesh> placementSubmeshes(numDrawCalls);

	for (uint32 i = 0; i < numDrawCalls; ++i)
	{
		placementSubmeshes[i].aabbMin = vec4(submeshes[i].aabb.min, 1.f);
		placementSubmeshes[i].aabbMax = vec4(submeshes[i].aabb.max, 1.f);

		commands[i].material.albedoTint = vec4(1.f, 1.f, 1.f, 1.f);
		commands[i].material.roughnessOverride = 1.f;
		commands[i].material.metallicOverride = 0.f;
		commands[i].material.textureID_usageFlags = submeshes[i].textureID_usageFlags;

		commands[i].drawArguments.IndexCountPerInstance = submeshes[i].numTriangles * 3;
		commands[i].drawArguments.StartIndexLocation = submeshes[i].firstTriangle * 3;
		commands[i].drawArguments.BaseVertexLocation = submeshes[i].baseVertex;

		// Will be set in shader.
		commands[i].drawArguments.InstanceCount = 0;
		commands[i].drawArguments.StartInstanceLocation = 0;

		depthOnlyCommands[i].drawArguments = commands[i].drawArguments;
	}




	
	samplePointsBuffer.initialize(device, POISSON_SAMPLES, arraysize(POISSON_SAMPLES), commandList);
	SET_NAME(samplePointsBuffer.resource, "Poisson Sample Points");

	meshBuffer.initialize(device, meshes.data(), (uint32)meshes.size(), commandList);
	SET_NAME(meshBuffer.resource, "Placement Meshes");

	submeshBuffer.initialize(device, placementSubmeshes.data(), (uint32)placementSubmeshes.size(), commandList);
	SET_NAME(submeshBuffer.resource, "Placement Submeshes");

	submeshCountBuffer.initialize<uint32>(device, nullptr, 512);
	SET_NAME(submeshCountBuffer.resource, "Placement Submesh Counts");

	submeshOffsetBuffer.initialize<uint32>(device, nullptr, 512);
	SET_NAME(submeshOffsetBuffer.resource, "Placement Submesh Offsets");

	placementPointsBuffer.initialize<placement_point>(device, nullptr, (uint32)tiles.size() * arraysize(POISSON_SAMPLES) * maxNumObjectFactor);
	SET_NAME(placementPointsBuffer.resource, "Placement Points");

	numPlacementPointsBuffer.initialize<uint32>(device, nullptr, 1);
	SET_NAME(numPlacementPointsBuffer.resource, "Placement Point Count");

	maxNumInstances =
		(uint32)tiles.size()
		* arraysize(POISSON_SAMPLES) // Each sample produces at maximum one of the specified meshes.
		* 4 // We allow maximum 4 submeshes per object.
		* maxNumObjectFactor;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		renderResources[i].commandBuffer.initialize<indirect_command>(device, commands.data(), numDrawCalls, commandList);
		SET_NAME(renderResources[i].commandBuffer.resource, "Placement Commands");

		renderResources[i].depthOnlyCommandBuffer.initialize<indirect_depth_only_command>(device, depthOnlyCommands.data(), numDrawCalls, commandList);
		SET_NAME(renderResources[i].depthOnlyCommandBuffer.resource, "Placement Depth Only Commands");

		renderResources[i].instanceBuffer.initialize<mat4>(device, nullptr, maxNumInstances);
		SET_NAME(renderResources[i].instanceBuffer.resource, "Placement Instances");
	}

}

void procedural_placement::generate(const render_camera& camera)
{
	PROFILE_FUNCTION();

	++currentRenderResources;
	if (currentRenderResources >= NUM_BUFFERED_FRAMES)
	{
		currentRenderResources = 0;
	}

	commandBuffer = renderResources[currentRenderResources].commandBuffer;
	depthOnlyCommandBuffer = renderResources[currentRenderResources].depthOnlyCommandBuffer;
	instanceBufferInternal = renderResources[currentRenderResources].instanceBuffer;
	instanceBuffer.resource = renderResources[currentRenderResources].instanceBuffer.resource;
	instanceBuffer.view.BufferLocation = instanceBuffer.resource->GetGPUVirtualAddress();
	instanceBuffer.view.SizeInBytes = maxNumInstances * sizeof(mat4);
	instanceBuffer.view.StrideInBytes = sizeof(mat4);

#if PROCEDURAL_PLACEMENT_ALLOW_SIMULTANEOUS_EDITING
	// This is only necessary, if we use the density textures on the render queue as well (I think).
	dx_command_queue::computeCommandQueue.waitForOtherQueue(dx_command_queue::renderCommandQueue);
#endif

	dx_command_list* commandList = dx_command_queue::computeCommandQueue.getAvailableCommandList();

	camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

	{
		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Generate procedural.");

		clearBuffer(commandList, numPlacementPointsBuffer);
		clearBuffer(commandList, submeshCountBuffer);

		uint32 maxNumGeneratedPlacementPoints = generatePoints(commandList, camera.position, frustum);
		computeSubmeshOffsets(commandList);

		if (maxNumGeneratedPlacementPoints > 0)
		{
			placeGeometry(commandList, frustum, maxNumGeneratedPlacementPoints);
		}

		createCommands(commandList);

		// These are the buffers used for rendering.
		commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(instanceBufferInternal.resource, D3D12_RESOURCE_STATE_COMMON);


#if PROCEDURAL_PLACEMENT_ALLOW_SIMULTANEOUS_EDITING
		transitionAllTexturesToCommon(commandList);
#endif


		/*commandList->transitionBarrier(submeshCountBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(submeshOffsetBuffer.resource, D3D12_RESOURCE_STATE_COMMON);*/
	}
	{
		PROFILE_BLOCK("Execute command list");
		dx_command_queue::computeCommandQueue.executeCommandList(commandList);
	}
	{
		PROFILE_BLOCK("Wait for compute queue");
		dx_command_queue::renderCommandQueue.waitForOtherQueue(dx_command_queue::computeCommandQueue);


		/*flushApplication();
		uint32 counts[512];
		uint32 offsets[512];
		indirect_command commands[32];
		uint32 numDraws[2];
		submeshCountBuffer.copyBackToCPU(counts, submeshCountBuffer.count * sizeof(uint32));
		submeshOffsetBuffer.copyBackToCPU(offsets, submeshOffsetBuffer.count * sizeof(uint32));
		commandBuffer.copyBackToCPU(commands, commandBuffer.count * sizeof(indirect_command));
		numDrawCallsBuffer.copyBackToCPU(numDraws, 2 * sizeof(uint32));
		flushApplication();
		int a = 0;*/
	}
}

void procedural_placement::transitionAllTexturesToCommon(dx_command_list* commandList)
{
	for (placement_tile& tile : tiles)
	{
		for (uint32 i = 0; i < placement_layer_count; ++i)
		{
			if (tile.layers[i].active)
			{
				commandList->transitionBarrier(tile.layers[i].densities, D3D12_RESOURCE_STATE_COMMON);
			}
		}
	}
}

void procedural_placement::clearBuffer(dx_command_list* commandList, dx_structured_buffer& buffer)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Clear buffer.");

	commandList->setPipelineState(clearBufferPipelineState);
	commandList->setComputeRootSignature(clearBufferRootSignature);

	commandList->setCompute32BitConstants(0, buffer.count);
	commandList->stageDescriptors(1, 0, 1, buffer.uav);

	commandList->dispatch(bucketize(buffer.count, 512), 1, 1);

	commandList->uavBarrier(buffer.resource);
}

uint32 procedural_placement::generatePoints(dx_command_list* commandList, vec3 cameraPosition, const camera_frustum_planes& frustum)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Generate procedural points.");

	commandList->setPipelineState(generatePointsPipelineState);
	commandList->setComputeRootSignature(generatePointsRootSignature);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, placementPointsBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, numPlacementPointsBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 2, 1, submeshCountBuffer.uav);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, samplePointsBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 2, 1, meshBuffer.srv);

	uint32 maxNumGeneratedPlacementPoints = 0;


	float diameterInUVSpace = radiusInUVSpace * 2.f;


	for (uint32 tileIndex = 0; tileIndex < (uint32)tiles.size(); ++tileIndex)
	{
		placement_tile& tile = tiles[tileIndex];

		vec2 corner(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);

		if (!frustum.cullWorldSpaceAABB(tile.aabb))
		{
			for (uint32 i = 0; i < placement_layer_count; ++i)
			{
				placement_layer& layer = tile.layers[i];
				placement_layer_description& desc = layerDescriptions[i];
				if (layer.active)
				{
					float footprint = desc.objectFootprint;
					float diameterInWorldSpace = diameterInUVSpace * PROCEDURAL_TILE_SIZE;

					float scaling = diameterInWorldSpace / footprint;
					uint32 numGroupsPerDim = (uint32)ceil(scaling);

					placement_gen_points_cb cb;
					cb.cameraPosition = vec4(cameraPosition, 1.f);
					cb.tileCorner = corner;
					cb.tileSize = PROCEDURAL_TILE_SIZE;
					cb.groundHeight = tile.groundHeight;
					for (uint32 o = 0; o < desc.numDensityMaps; ++o)
					{
						cb.options[o] = (desc.options[o].offset << 16) | (desc.options[o].count);
					}
					cb.numDensityMaps = desc.numDensityMaps;
					cb.uvScale = 1.f / scaling;
					cb.uvOffset = 1.f / scaling;

					maxNumGeneratedPlacementPoints += numGroupsPerDim * numGroupsPerDim * arraysize(POISSON_SAMPLES);

					commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, cb);

					commandList->setShaderResourceView(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1, layer.densities, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


					commandList->dispatch(numGroupsPerDim, numGroupsPerDim, 1);

					commandList->uavBarrier(placementPointsBuffer.resource);
					commandList->uavBarrier(numPlacementPointsBuffer.resource);
					commandList->uavBarrier(submeshCountBuffer.resource);
				}
			}
		}
	}

	return maxNumGeneratedPlacementPoints;
}

void procedural_placement::computeSubmeshOffsets(dx_command_list* commandList)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Compute submesh offsets.");

	commandList->setPipelineState(prefixSumPipelineState);
	commandList->setComputeRootSignature(prefixSumRootSignature);

	commandList->transitionBarrier(submeshCountBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(submeshOffsetBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, submeshCountBuffer.count);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, submeshCountBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, submeshOffsetBuffer.srv);

	commandList->dispatch(1, 1, 1);

	commandList->uavBarrier(submeshOffsetBuffer.resource);	
}

void procedural_placement::placeGeometry(dx_command_list* commandList, const camera_frustum_planes& frustum, uint32 maxNumGeneratedPlacementPoints)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Place procedural geometry.");


	commandList->setPipelineState(placeGeometryPipelineState);
	commandList->setComputeRootSignature(placeGeometryRootSignature);

	commandList->transitionBarrier(submeshOffsetBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->transitionBarrier(instanceBufferInternal.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CAMERA, frustum);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, placementPointsBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1, 1, meshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 2, 1, submeshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 3, 1, submeshOffsetBuffer.srv);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, numPlacementPointsBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, submeshCountBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 2, 1, instanceBufferInternal.uav);

	commandList->dispatch(bucketize(maxNumGeneratedPlacementPoints, 512), 1, 1);

	commandList->uavBarrier(numPlacementPointsBuffer.resource);
	commandList->uavBarrier(submeshCountBuffer.resource);
	commandList->uavBarrier(instanceBufferInternal.resource);
}

void procedural_placement::createCommands(dx_command_list* commandList)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Create procedural commands.");


	commandList->setPipelineState(createCommandsPipelineState);
	commandList->setComputeRootSignature(createCommandsRootSignature);

	commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(submeshOffsetBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, submeshBuffer.count);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, submeshCountBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1, 1, submeshOffsetBuffer.srv);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, commandBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, depthOnlyCommandBuffer.uav);

	commandList->dispatch(bucketize(submeshBuffer.count, 512), 1, 1);

	commandList->uavBarrier(commandBuffer.resource);
	commandList->uavBarrier(depthOnlyCommandBuffer.resource);
}

void placement_tile::allocateLayer(placement_layer_name layerName)
{
	if (layers[layerName].active)
	{
		return;
	}

	placement_layer& layer = layers[layerName];
	layer.active = true;


	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, 512, 512);
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.MipLevels = 1;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = desc.Format;
	clearValue.Color[0] = 0.f;
	clearValue.Color[1] = 0.f;
	clearValue.Color[2] = 0.f;
	clearValue.Color[3] = 0.f;

	layer.densities.initialize(device, desc, &clearValue);
	dx_resource_state_tracker::addGlobalResourceState(layer.densities.resource.Get(), D3D12_RESOURCE_STATE_COMMON, 1);
	
}
