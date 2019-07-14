#include "pch.h"
#include "game.h"
#include "commands.h"
#include "error.h"
#include "model.h"

#include <d3dcompiler.h>



void loadScene(ComPtr<ID3D12Device2> device, ComPtr<ID3D12PipelineState>& pipelineState, scene_data& result)
{
	cpu_mesh_group<vertex_3PUN> model;
	model.loadFromFile("res/big_oak.obj");

	dx_command_queue& copyCommandQueue = dx_command_queue::copyCommandQueue;
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();


	result.textures.resize(model.meshes.size());
	for (uint32 i = 0; i < model.meshes.size(); ++i)
	{
		cpu_mesh<vertex_3PUN>& mesh = model.meshes[i];
		result.meshes.push_back(commandList->createMesh(mesh));
		commandList->loadTextureFromFile(result.textures[i], mesh.material.albedo, texture_usage_albedo);
	}

	auto mesh = cpu_mesh<vertex_3PUN>::capsule(20, 21, 3, 2);
	result.quadMesh = commandList->createMesh(mesh);

	uint64 fenceValue = copyCommandQueue.executeCommandList(commandList);
	copyCommandQueue.waitForFenceValue(fenceValue);
}

void dx_game::initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height)
{
	this->device = device;
	scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	checkResult(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/VertexShader.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/PixelShader.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// Create a root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstants(16 * 3, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // 4 * 16 floats (mat4).
	rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler(0,
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignature.initialize(device, rootSignatureDesc);



	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.dsvFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.rtvFormats = rtvFormats;
	pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE, // Disable backface culling for the leaves.
		TRUE, // Righthanded coordinate system.
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,
		FALSE, 
		FALSE,
		0,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
		);

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));



	loadScene(device, pipelineState, scene);

	contentLoaded = true;

	this->width = width;
	this->height = height;
	resizeDepthBuffer(width, height);
}

void dx_game::resize(uint32 width, uint32 height)
{
	if (width != this->width || height != this->height)
	{
		this->width = width;
		this->height = height;
		viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);
		resizeDepthBuffer(width, height);
	}
}

void dx_game::resizeDepthBuffer(uint32 width, uint32 height)
{
	if (contentLoaded)
	{
		// Flush any GPU commands that might be referencing the depth buffer.
		flushApplication();

		width = max(1u, width);
		height = max(1u, height);

		// Resize screen dependent resources.
		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		checkResult(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
				1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&depthBuffer)
		));

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(depthBuffer.Get(), &dsv,
			dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
}

void dx_game::update(float dt)
{
	static float totalTime = 0.f;
	totalTime += dt;
	float angle = totalTime * 45.f;
	vec3 rotationAxis(0, 1, 0);
	modelMatrix = mat4::CreateFromAxisAngle(rotationAxis, DirectX::XMConvertToRadians(angle));

	vec3 eyePosition(0, 3, 6);
	vec3 focusPoint(0, 2, 0);
	vec3 upDirection(0, 1, 0);
	viewMatrix = mat4::CreateLookAt(eyePosition, focusPoint, upDirection);

	float aspectRatio = (float)width / (float)height;
	projectionMatrix = mat4::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(70.f), aspectRatio, 0.1f, 100.0f);
}

void dx_game::render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtv)
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();

	FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };


	// Clear.
	commandList->clearRTV(rtv, clearColor);
	commandList->clearDepth(dsv);

	// Prepare for rendering.
	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(rootSignature);

	commandList->setRenderTarget(&rtv, 1, &dsv);
	commandList->setViewport(viewport);
	commandList->setScissor(scissorRect);
	
	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	struct cb
	{
		mat4 modelMatrix;
		mat4 viewMatrix;
		mat4 projMatrix;
	} c = {
		modelMatrix, viewMatrix, projectionMatrix
	};

	commandList->setGraphics32BitConstants(0, c);

	for (uint32 i = 0; i < scene.meshes.size(); ++i)
	{
		commandList->setVertexBuffer(0, scene.meshes[i].vertexBuffer);
		commandList->setIndexBuffer(scene.meshes[i].indexBuffer);
		commandList->setShaderResourceView(1, 0, scene.textures[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// Render.
		commandList->drawIndexed(scene.meshes[i].indexBuffer.numIndices, 1, 0, 0, 0);
	}

	//commandList->setVertexBuffer(0, scene.quadMesh.vertexBuffer);
	//commandList->setIndexBuffer(scene.quadMesh.indexBuffer);
	//commandList->setShaderResourceView(1, 0, scene.textures[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//commandList->drawIndexed(scene.quadMesh.indexBuffer.numIndices, 1, 0, 0, 0);
}


