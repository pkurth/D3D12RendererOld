#include "pch.h"
#include "procedural_placement.h"
#include "error.h"
#include "graphics.h"
#include "indirect_drawing.h"
#include "command_queue.h"
#include "profiling.h"
#include "poisson_distribution.h"

void procedural_placement::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList)
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

		CD3DX12_DESCRIPTOR_RANGE1 density(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		CD3DX12_DESCRIPTOR_RANGE1 placementPoints(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_CB].InitAsConstants(sizeof(placement_gen_points_cb) / sizeof(float), 0);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS].InitAsDescriptorTable(1, &placementPoints);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_DENSITY].InitAsDescriptorTable(1, &density);

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

		CD3DX12_DESCRIPTOR_RANGE1 placementPoints(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		CD3DX12_DESCRIPTOR_RANGE1 commands(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_CB].InitAsConstants(sizeof(placement_place_geometry_cb) / sizeof(float), 0);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS].InitAsDescriptorTable(1, &placementPoints);
		rootParameters[PROCEDURAL_PLACEMENT_ROOTPARAM_COMMANDS].InitAsDescriptorTable(1, &commands);

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


	for (uint32 i = 0; i < arraysize(POISSON_SAMPLES); ++i)
	{
		POISSON_SAMPLES[i].z = randomFloat(0.f, 1.f);
	}

	maxNumDrawCalls = arraysize(POISSON_SAMPLES) * 4; // * 4, because multiple meshes per object.
	placementPointBuffer.initialize<placement_point>(device, nullptr, maxNumDrawCalls);
	poissonSampleBuffer.initialize(device, POISSON_SAMPLES, arraysize(POISSON_SAMPLES), commandList);

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		renderResources[i].numDrawCallsBuffer.initialize<uint32>(device, nullptr, 1);
		renderResources[i].commandBuffer.initialize<indirect_command>(device, nullptr, maxNumDrawCalls);
		renderResources[i].depthOnlyCommandBuffer.initialize<indirect_depth_only_command>(device, nullptr, maxNumDrawCalls);
	}
}

void procedural_placement::generate(const render_camera& camera, dx_texture& densityMap,
	placement_mesh* meshes, uint32 numMeshes, float dt)
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

	clearCount(commandList);
	generatePoints(commandList, densityMap, numMeshes, dt);
	placeGeometry(commandList, meshes, numMeshes);

	dx_command_queue::computeCommandQueue.executeCommandList(commandList);
	
	{
		PROFILE_BLOCK("Wait for compute queue");
		dx_command_queue::renderCommandQueue.waitForOtherQueue(dx_command_queue::computeCommandQueue);
	}
}

void procedural_placement::clearCount(dx_command_list* commandList)
{
	PROFILE_FUNCTION();

	commandList->setPipelineState(clearCountPipelineState);
	commandList->setComputeRootSignature(clearCountRootSignature);

	commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->stageDescriptors(0, 0, 1, numDrawCallsBuffer.uav);

	commandList->dispatch(1, 1, 1);

	commandList->uavBarrier(numDrawCallsBuffer.resource);
}

void procedural_placement::generatePoints(dx_command_list* commandList, dx_texture& densityMap, uint32 numMeshes, float dt)
{
	PROFILE_FUNCTION();

	static float time = 0.f;
	time += dt;

	placement_gen_points_cb cb;
	cb.numMeshes = numMeshes;
	cb.time = time;

	commandList->setPipelineState(generatePointsPipelineState);
	commandList->setComputeRootSignature(generatePointsRootSignature);

	commandList->transitionBarrier(placementPointBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(poissonSampleBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, cb);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS, 0, 1, placementPointBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS, 1, 1, numDrawCallsBuffer.uav);
	commandList->setShaderResourceView(PROCEDURAL_PLACEMENT_ROOTPARAM_DENSITY, 0, densityMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_DENSITY, 1, 1, poissonSampleBuffer.srv);

	commandList->dispatch((uint32)bucketize(arraysize(POISSON_SAMPLES), 512), 1, 1);

	commandList->uavBarrier(placementPointBuffer.resource);
	commandList->uavBarrier(numDrawCallsBuffer.resource);
}

void procedural_placement::placeGeometry(dx_command_list* commandList, placement_mesh* meshes, uint32 numMeshes)
{
	PROFILE_FUNCTION();

	assert(numMeshes > 0);

	placement_place_geometry_cb cb;
	cb.mesh0 = meshes[0];
	if (numMeshes > 1)
	{
		cb.mesh1 = meshes[1];
	}
	if (numMeshes > 2)
	{
		cb.mesh2 = meshes[2];
	}
	if (numMeshes > 3)
	{
		cb.mesh3 = meshes[3];
	}
	cb.numMeshes = min(4u, numMeshes);

	commandList->setPipelineState(placeGeometryPipelineState);
	commandList->setComputeRootSignature(placeGeometryRootSignature);

	commandList->transitionBarrier(placementPointBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(numDrawCallsBuffer.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->setCompute32BitConstants(PROCEDURAL_PLACEMENT_ROOTPARAM_CB, cb);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS, 0, 1, placementPointBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_POINTS, 1, 1, numDrawCallsBuffer.srv);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_COMMANDS, 0, 1, commandBuffer.uav);
	commandList->stageDescriptors(PROCEDURAL_PLACEMENT_ROOTPARAM_COMMANDS, 1, 1, depthOnlyCommandBuffer.uav);

	commandList->dispatch(bucketize(maxNumDrawCalls, 512), 1, 1);

	//commandList->uavBarrier(commandBuffer.resource);
	//commandList->uavBarrier(depthOnlyCommandBuffer.resource);

	commandList->transitionBarrier(commandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
	commandList->transitionBarrier(depthOnlyCommandBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
}
