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
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_clear_count.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 count(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsDescriptorTable(1, &count);


		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		clearCountRootSignature.initialize(device, rootSignatureDesc);

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_CS cs;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = clearCountRootSignature.rootSignature.Get();
		pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&clearCountPipelineState)));

		SET_NAME(clearCountRootSignature.rootSignature, "Procedural Placement Clear Count Root Signature");
		SET_NAME(clearCountPipelineState, "Procedural Placement Clear Count Pipeline");
	}

	{
		ComPtr<ID3DBlob> shaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/procedural_placement_gen_points.cso", &shaderBlob));

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0); // Density map(s) and poisson samples.
		CD3DX12_DESCRIPTOR_RANGE1 uavs(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Generated points and point count.

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

		CD3DX12_DESCRIPTOR_RANGE1 srvs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
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




	for (uint32 i = 0; i < arraysize(POISSON_SAMPLES); ++i)
	{
		POISSON_SAMPLES[i].z = randomFloat(0.f, 1.f);
	}

	poissonSampleBuffer.initialize(device, POISSON_SAMPLES, arraysize(POISSON_SAMPLES), commandList);

	meshBuffer.initialize(device, meshes.data(), (uint32)meshes.size(), commandList);
	submeshBuffer.initialize(device, subMeshes.data(), (uint32)subMeshes.size(), commandList);


	placementPointBuffer.initialize<placement_point>(device, nullptr, (uint32)tiles.size() * arraysize(POISSON_SAMPLES) * maxNumObjectFactor);


	maxNumDrawCalls =
		(uint32)tiles.size()
		* arraysize(POISSON_SAMPLES) // Each sample produces at maximum one of the specified meshes.
		* 4 // We allow maximum 4 submeshes per object.
		* maxNumObjectFactor;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		renderResources[i].numDrawCallsBuffer.initialize<uint32>(device, nullptr, 2);
		renderResources[i].commandBuffer.initialize<indirect_command>(device, nullptr, maxNumDrawCalls);
		renderResources[i].depthOnlyCommandBuffer.initialize<indirect_depth_only_command>(device, nullptr, maxNumDrawCalls);
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

	dx_command_list* commandList = dx_command_queue::computeCommandQueue.getAvailableCommandList();

	camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

	{
		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Generate procedural.");

		clearCount(commandList);
		uint32 maxNumGeneratedPlacementPoints = generatePoints(commandList, frustum);

		if (maxNumGeneratedPlacementPoints > 0)
		{
			placeGeometry(commandList, frustum, maxNumGeneratedPlacementPoints);
		}

		commandList->uavBarrier(numDrawCallsBuffer.resource);

		commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
		commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
	}
	dx_command_queue::computeCommandQueue.executeCommandList(commandList);
	
	{
		PROFILE_BLOCK("Wait for compute queue");
		dx_command_queue::renderCommandQueue.waitForOtherQueue(dx_command_queue::computeCommandQueue);
	}
}

void procedural_placement::clearCount(dx_command_list* commandList)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Clear procedural count.");

	commandList->setPipelineState(clearCountPipelineState);
	commandList->setComputeRootSignature(clearCountRootSignature);

	commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->stageDescriptors(0, 0, 1, numDrawCallsBuffer.uav);

	commandList->dispatch(1, 1, 1);

	commandList->uavBarrier(numDrawCallsBuffer.resource);
}

uint32 procedural_placement::generatePoints(dx_command_list* commandList, const camera_frustum_planes& frustum)
{
	PROFILE_FUNCTION();
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Generate procedural points.");

	commandList->setPipelineState(generatePointsPipelineState);
	commandList->setComputeRootSignature(generatePointsRootSignature);

	commandList->transitionBarrier(placementPointBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(poissonSampleBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, placementPointBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, numDrawCallsBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, poissonSampleBuffer.srv);

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

		}
	}

	return maxNumGeneratedPlacementPoints;
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

	commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CAMERA, frustum);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 0, 1, placementPointBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 1, 1, meshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_SRVS, 2, 1, submeshBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 0, 1, commandBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 1, 1, depthOnlyCommandBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_UAVS, 2, 1, numDrawCallsBuffer.uav);

	commandList->dispatch(bucketize(maxNumGeneratedPlacementPoints, 512), 1, 1);
}
