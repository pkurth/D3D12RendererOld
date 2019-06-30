#include "game.h"
#include "commands.h"
#include "error.h"

#include <d3dcompiler.h>


struct vertex_3PC
{
	vec3 position;
	vec3 color;
	vec2 uv;
};

#if 0
static vertex_3PC cubeVertices[8] = {
	{ vec3(-1.0f, -1.0f, -1.0f), vec3(0.0f, 0.0f, 0.0f), vec2() }, // 0
	{ vec3(-1.0f,  1.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f), vec2() }, // 1
	{ vec3(1.0f,  1.0f, -1.0f), vec3(1.0f, 1.0f, 0.0f), vec2() }, // 2
	{ vec3(1.0f, -1.0f, -1.0f), vec3(1.0f, 0.0f, 0.0f), vec2() }, // 3
	{ vec3(-1.0f, -1.0f,  1.0f), vec3(0.0f, 0.0f, 1.0f), vec2() }, // 4
	{ vec3(-1.0f,  1.0f,  1.0f), vec3(0.0f, 1.0f, 1.0f), vec2() }, // 5
	{ vec3(1.0f,  1.0f,  1.0f), vec3(1.0f, 1.0f, 1.0f), vec2() }, // 6
	{ vec3(1.0f, -1.0f,  1.0f), vec3(1.0f, 0.0f, 1.0f), vec2() }  // 7
};

static uint16 cubeIndices[36] =
{
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};
#else
static vertex_3PC cubeVertices[] = {
	{ vec3(-1.f, -1.f, 0.f), vec3(1.f, 1.f, 1.f), vec2(0.f, 0.f) },
	{ vec3(1.f, -1.f, 0.f), vec3(1.f, 1.f, 1.f), vec2(1.f, 0.f) },
	{ vec3(1.f, 1.f, 0.f), vec3(1.f, 1.f, 1.f), vec2(1.f, 1.f) },
	{ vec3(-1.f, 1.f, 0.f), vec3(1.f, 1.f, 1.f), vec2(0.f, 1.f) },
};

static uint16 cubeIndices[] =
{
	0, 2, 1,
	0, 3, 2,
};
#endif

void loadScene(ComPtr<ID3D12Device2> device, ComPtr<ID3D12PipelineState>& pipelineState, scene_data& result)
{
	dx_command_queue& copyCommandQueue = dx_command_queue::copyCommandQueue;
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();

	result.vertexBuffer = commandList->createVertexBuffer(cubeVertices, arraysize(cubeVertices));
	result.indexBuffer = commandList->createIndexBuffer(cubeIndices, arraysize(cubeIndices));
	commandList->loadTextureFromFile(result.texture, L"res/bus.png", texture_usage_albedo);

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/VertexShader.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/PixelShader.cso", &pixelShaderBlob));

	uint64 fenceValue = copyCommandQueue.executeCommandList(commandList);

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


	// Create a root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // 16 floats (mat4).
	rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, 
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, 
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &linearClampSampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	result.rootSignature.initialize(device, rootSignatureDesc);



	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipelineStateStream.rootSignature = result.rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.dsvFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.rtvFormats = rtvFormats;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));


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
	float angle = totalTime * 90.f;
	const vec4 rotationAxis = DirectX::XMVectorSet(0, 1, 1, 0);

	modelMatrix = DirectX::XMMatrixRotationAxis(rotationAxis, DirectX::XMConvertToRadians(angle));

	const vec4 eyePosition = DirectX::XMVectorSet(0, 0, -10, 1);
	const vec4 focusPoint = DirectX::XMVectorSet(0, 0, 0, 1);
	const vec4 upDirection = DirectX::XMVectorSet(0, 1, 0, 0);
	viewMatrix = DirectX::XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	float aspectRatio = (float)width / (float)height;
	projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(70.f), aspectRatio, 0.1f, 100.0f);
}

void dx_game::render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtv)
{
	auto dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();

	FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

	// Clear.
	commandList->clearRTV(rtv, clearColor);
	commandList->clearDepth(dsv);

	// Prepare for rendering.
	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(scene.rootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->setVertexBuffer(0, scene.vertexBuffer);
	commandList->setIndexBuffer(scene.indexBuffer);

	commandList->setViewport(viewport);
	commandList->setScissor(scissorRect);

	mat4 mvpMatrix = XMMatrixMultiply(modelMatrix, viewMatrix);
	mvpMatrix = XMMatrixMultiply(mvpMatrix, projectionMatrix);
	commandList->setGraphics32BitConstants(0, mvpMatrix);
	commandList->setShaderResourceView(1, 0, scene.texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	ComPtr<ID3D12GraphicsCommandList2> d3d12CommandList = commandList->getD3D12CommandList();
	d3d12CommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);


	// Render.
	commandList->drawIndexed(arraysize(cubeIndices), 1, 0, 0, 0);
}


