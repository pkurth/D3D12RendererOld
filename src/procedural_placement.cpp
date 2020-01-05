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
	vec2 tileCorner;
	float tileSize;
	uint32 numDensityMaps;
	uint32 meshOffset;
	float groundHeight;

	float uvScale;
	float uvOffset;
};

struct placement_point
{
	vec4 position;
	vec4 normal;
};

// Dither algorithms:
// https://www.shadertoy.com/view/XlyXWW

void procedural_placement::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList,
	std::vector<placement_tile>& tiles, std::vector<submesh_info>& subMeshes)
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

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0); // Sample positions, density map(s) and meshes.
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

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
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

	this->tiles = tiles;

	std::vector<placement_mesh> meshes;
	meshes.reserve(tiles.size() * 4);
	for (uint32 i = 0; i < (uint32)tiles.size(); ++i)
	{
		placement_tile& tile = this->tiles[i];
		assert(tile.numMeshes > 0);
		assert(tile.numMeshes <= 4);

		tile.meshOffset = (uint32)meshes.size();

		for (uint32 j = 0; j < tile.numMeshes; ++j)
		{
			meshes.push_back(tile.meshes[j]);
		}

		tile.objectFootprint = max(tile.objectFootprint, PROCEDURAL_MIN_FOOTPRINT);
	}

	radiusInUVSpace = sqrt(1.f / (2.f * sqrt(3.f) * arraysize(POISSON_SAMPLES)));
	float diameterInUVSpace = radiusInUVSpace * 2.f;

	float diameterInWorldSpace = diameterInUVSpace * PROCEDURAL_TILE_SIZE;

	float scaling = diameterInWorldSpace / PROCEDURAL_MIN_FOOTPRINT;
	uint32 numGroupsPerDim = (uint32)ceil(scaling);
	uint32 maxNumObjectFactor = numGroupsPerDim * numGroupsPerDim;



	
	samplePointsBuffer.initialize(device, POISSON_SAMPLES, arraysize(POISSON_SAMPLES), commandList);

	meshBuffer.initialize(device, meshes.data(), (uint32)meshes.size(), commandList);
	submeshBuffer.initialize(device, subMeshes.data(), (uint32)subMeshes.size(), commandList);
	submeshCountBuffer.initialize<uint32>(device, nullptr, 512);
	submeshOffsetBuffer.initialize<uint32>(device, nullptr, 512);

	placementPointBuffer.initialize<placement_point>(device, nullptr, (uint32)tiles.size() * arraysize(POISSON_SAMPLES) * maxNumObjectFactor);

	maxNumDrawCalls = (uint32)subMeshes.size();

	maxNumInstances =
		(uint32)tiles.size()
		* arraysize(POISSON_SAMPLES) // Each sample produces at maximum one of the specified meshes.
		* 4 // We allow maximum 4 submeshes per object.
		* maxNumObjectFactor;

	std::vector<mat4> instanceData(maxNumInstances);
	for (uint32 i = 0; i < maxNumInstances; ++i)
	{
		instanceData[i] = createModelMatrix(vec3(i * 3.f, 0.f, 0.f), quat::identity, 1.f);
	}

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		renderResources[i].numDrawCallsBuffer.initialize<uint32>(device, nullptr, 2);
		renderResources[i].commandBuffer.initialize<indirect_command>(device, nullptr, maxNumDrawCalls);
		renderResources[i].depthOnlyCommandBuffer.initialize<indirect_depth_only_command>(device, nullptr, maxNumDrawCalls);
		renderResources[i].instanceBuffer.initialize(device, instanceData.data(), maxNumInstances, commandList);
	}





	dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);
	defaultSRV = allocation.getDescriptorHandle(0);

	for (uint32 i = 0; i < 4; ++i)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = -1;

		device->CreateShaderResourceView(
			nullptr, &srvDesc,
			allocation.getDescriptorHandle(i)
		);
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

	numDrawCallsBuffer = renderResources[currentRenderResources].numDrawCallsBuffer;
	commandBuffer = renderResources[currentRenderResources].commandBuffer;
	depthOnlyCommandBuffer = renderResources[currentRenderResources].depthOnlyCommandBuffer;
	instanceBufferInternal = renderResources[currentRenderResources].instanceBuffer;
	instanceBuffer.resource = renderResources[currentRenderResources].instanceBuffer.resource;
	instanceBuffer.view.BufferLocation = instanceBuffer.resource->GetGPUVirtualAddress();
	instanceBuffer.view.SizeInBytes = maxNumInstances * sizeof(mat4);
	instanceBuffer.view.StrideInBytes = sizeof(mat4);

	dx_command_list* commandList = dx_command_queue::computeCommandQueue.getAvailableCommandList();

	camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

	{
		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Generate procedural.");

		clearBuffer(commandList, numDrawCallsBuffer);
		clearBuffer(commandList, submeshCountBuffer);

		uint32 maxNumGeneratedPlacementPoints = generatePoints(commandList, camera.position, frustum);
		computeSubmeshOffsets(commandList);

		if (maxNumGeneratedPlacementPoints > 0)
		{
			placeGeometry(commandList, frustum, maxNumGeneratedPlacementPoints);
		}

		createCommands(commandList);

		commandList->uavBarrier(numDrawCallsBuffer.resource);

		commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(instanceBufferInternal.resource, D3D12_RESOURCE_STATE_COMMON);



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

void procedural_placement::clearBuffer(dx_command_list* commandList, dx_structured_buffer& buffer)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Clear buffer.");

	commandList->setPipelineState(clearBufferPipelineState);
	commandList->setComputeRootSignature(clearBufferRootSignature);

	commandList->transitionBarrier(buffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

	commandList->transitionBarrier(placementPointBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(samplePointsBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, placementPointBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, numDrawCallsBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 2, 1, submeshCountBuffer.uav);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, samplePointsBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 5, 1, meshBuffer.srv);

	uint32 maxNumGeneratedPlacementPoints = 0;


	float diameterInUVSpace = radiusInUVSpace * 2.f;


	for (uint32 tileIndex = 0; tileIndex < (uint32)tiles.size(); ++tileIndex)
	{
		placement_tile& tile = tiles[tileIndex];

		bounding_box tileBB = bounding_box::negativeInfinity();
		vec2 corner(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);
		tileBB.grow(vec3(corner.x, tile.groundHeight, corner.y));
		tileBB.grow(vec3(corner.x + PROCEDURAL_TILE_SIZE, tile.groundHeight + tile.maximumHeight, corner.y + PROCEDURAL_TILE_SIZE));

		if (!frustum.cullWorldSpaceAABB(tileBB))
		{
			float footprint = tile.objectFootprint;
			float diameterInWorldSpace = diameterInUVSpace * PROCEDURAL_TILE_SIZE;

			float scaling = diameterInWorldSpace / footprint;
			uint32 numGroupsPerDim = (uint32)ceil(scaling);

			placement_gen_points_cb cb;
			cb.cameraPosition = vec4(cameraPosition, 1.f);
			cb.tileCorner = corner;
			cb.tileSize = PROCEDURAL_TILE_SIZE;
			cb.groundHeight = tile.groundHeight;
			cb.meshOffset = tile.meshOffset;
			cb.numDensityMaps = tile.numMeshes;
			cb.uvScale = 1.f / scaling;
			cb.uvOffset = 1.f / scaling;

			maxNumGeneratedPlacementPoints += numGroupsPerDim * numGroupsPerDim * arraysize(POISSON_SAMPLES);

			commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, cb);

			for (uint32 i = 0; i < tile.numMeshes; ++i)
			{
				commandList->setShaderResourceView(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1 + i, *tile.densities[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}

			commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1 + tile.numMeshes, 4 - tile.numMeshes, defaultSRV);


			commandList->dispatch(numGroupsPerDim, numGroupsPerDim, 1);

			commandList->uavBarrier(placementPointBuffer.resource);
			commandList->uavBarrier(numDrawCallsBuffer.resource);
			commandList->uavBarrier(submeshCountBuffer.resource);

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
	
	commandList->uavBarrier(submeshOffsetBuffer.resource);
}

void procedural_placement::placeGeometry(dx_command_list* commandList, const camera_frustum_planes& frustum, uint32 maxNumGeneratedPlacementPoints)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Place procedural geometry.");


	commandList->setPipelineState(placeGeometryPipelineState);
	commandList->setComputeRootSignature(placeGeometryRootSignature);

	commandList->transitionBarrier(placementPointBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(meshBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(submeshBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(submeshOffsetBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->transitionBarrier(instanceBufferInternal.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CAMERA, frustum);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, placementPointBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1, 1, meshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 2, 1, submeshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 3, 1, submeshOffsetBuffer.srv);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, numDrawCallsBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, submeshCountBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 2, 1, instanceBufferInternal.uav);

	commandList->dispatch(bucketize(maxNumGeneratedPlacementPoints, 512), 1, 1);

	commandList->uavBarrier(numDrawCallsBuffer.resource);
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
	commandList->transitionBarrier(submeshCountBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, submeshBuffer.count);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, submeshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1, 1, submeshCountBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 2, 1, submeshOffsetBuffer.srv);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, commandBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, depthOnlyCommandBuffer.uav);

	commandList->dispatch(bucketize(submeshBuffer.count, 512), 1, 1);

	commandList->uavBarrier(commandBuffer.resource);
	commandList->uavBarrier(depthOnlyCommandBuffer.resource);
}
