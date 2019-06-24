#include "game.h"
#include "commands.h"
#include "error.h"

#include <d3dcompiler.h>


struct vertex_3PC
{
	vec3 position;
	vec3 color;
};

static vertex_3PC cubeVertices[8] = {
	{ vec3(-1.0f, -1.0f, -1.0f), vec3(0.0f, 0.0f, 0.0f) }, // 0
	{ vec3(-1.0f,  1.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f) }, // 1
	{ vec3(1.0f,  1.0f, -1.0f), vec3(1.0f, 1.0f, 0.0f) }, // 2
	{ vec3(1.0f, -1.0f, -1.0f), vec3(1.0f, 0.0f, 0.0f) }, // 3
	{ vec3(-1.0f, -1.0f,  1.0f), vec3(0.0f, 0.0f, 1.0f) }, // 4
	{ vec3(-1.0f,  1.0f,  1.0f), vec3(0.0f, 1.0f, 1.0f) }, // 5
	{ vec3(1.0f,  1.0f,  1.0f), vec3(1.0f, 1.0f, 1.0f) }, // 6
	{ vec3(1.0f, -1.0f,  1.0f), vec3(1.0f, 0.0f, 1.0f) }  // 7
};

static uint16 cubeIndicies[36] =
{
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

scene_data loadScene(ComPtr<ID3D12Device2> device, dx_command_queue& copyCommandQueue, ComPtr<ID3D12PipelineState>& pipelineState)
{
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();

	scene_data result;

	ComPtr<ID3D12Resource> intermediateVertexBuffer, intermediateIndexBuffer;
	result.vertexBuffer = createVertexBuffer(device, commandList, cubeVertices, arraysize(cubeVertices), intermediateVertexBuffer);
	result.indexBuffer = createIndexBuffer(device, commandList, cubeIndicies, arraysize(cubeIndicies), intermediateIndexBuffer);

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/VertexShader.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/PixelShader.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


	// Create a root signature.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// A single 32-bit constant root parameter that is used by the vertex shader.
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstants(sizeof(mat4) / 16 * sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(arraysize(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	// Serialize the root signature.
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	checkResult(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
		featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
	// Create the root signature.
	checkResult(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&result.rootSignature)));


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

	pipelineStateStream.rootSignature = result.rootSignature.Get();
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


	uint64 fenceValue = copyCommandQueue.executeCommandList(commandList);
	copyCommandQueue.waitForFenceValue(fenceValue);

	return result;
}

void dx_game::initialize(ComPtr<ID3D12Device2> device, dx_command_queue& copyCommandQueue, uint32 width, uint32 height)
{
	this->device = device;
	scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	checkResult(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));

	scene = loadScene(device, copyCommandQueue, pipelineState);

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

	commandList->clearRTV(rtv, clearColor);
	commandList->clearDepth(dsv);

	// Prepare for rendering.
	ComPtr<ID3D12GraphicsCommandList2> d3d12CommandList = commandList->getD3D12CommandList();
	d3d12CommandList->SetPipelineState(pipelineState.Get());
	d3d12CommandList->SetGraphicsRootSignature(scene.rootSignature.Get());

	d3d12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3d12CommandList->IASetVertexBuffers(0, 1, &scene.vertexBuffer.view);
	d3d12CommandList->IASetIndexBuffer(&scene.indexBuffer.view);

	d3d12CommandList->RSSetViewports(1, &viewport);
	d3d12CommandList->RSSetScissorRects(1, &scissorRect);

	d3d12CommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	mat4 mvpMatrix = XMMatrixMultiply(modelMatrix, viewMatrix);
	mvpMatrix = XMMatrixMultiply(mvpMatrix, projectionMatrix);
	d3d12CommandList->SetGraphicsRoot32BitConstants(0, sizeof(mat4) / 4, &mvpMatrix, 0);

	// Render.
	d3d12CommandList->DrawIndexedInstanced(arraysize(cubeIndicies), 1, 0, 0, 0);
}


