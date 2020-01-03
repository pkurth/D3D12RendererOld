#include "pch.h"
#include "debug_display.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void debug_display::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget)
{
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_textured_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_textured_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[DEBUG_DISPLAY_ROOTPARAM_CB].InitAsConstants((sizeof(mat4) + sizeof(vec4)) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[DEBUG_DISPLAY_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		unlitTexturedRootSignature.initialize(device, rootSignatureDesc);


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
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = unlitTexturedRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
		pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;
		pipelineStateStream.blend = alphaBlendDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&unlitTexturedPipelineState)));

		SET_NAME(unlitTexturedRootSignature.rootSignature, "Unlit Textured Root Signature");
		SET_NAME(unlitTexturedPipelineState, "Unlit Textured Pipeline");
	}

	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_flat_vs.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_flat_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[DEBUG_DISPLAY_ROOTPARAM_CB].InitAsConstants((sizeof(mat4) + sizeof(vec4)) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // MVP matrix.

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		unlitFlatRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = unlitFlatRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
		pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;

		{
			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(pipeline_state_stream), &pipelineStateStream
			};
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&unlitLinePipelineState)));
		}

		{
			pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(pipeline_state_stream), &pipelineStateStream
			};
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&unlitFlatPipelineState)));
		}

		SET_NAME(unlitFlatRootSignature.rootSignature, "Unlit Line Root Signature");
		SET_NAME(unlitLinePipelineState, "Unlit Line Pipeline");
		SET_NAME(unlitFlatPipelineState, "Unlit Flat Pipeline");
	}

	uint16 frustumIndices[] = {
		0, 1,
		0, 2,
		1, 3,
		2, 3,
		4, 5,
		4, 6,
		5, 7,
		6, 7,
		0, 4,
		1, 5,
		2, 6,
		3, 7,
	};

	frustumIndexBuffer.initialize(device, frustumIndices, arraysize(frustumIndices), commandList);
}

void debug_display::renderBillboard(dx_command_list* commandList, const render_camera& camera, vec3 position, vec2 dimensions, dx_texture& texture,
	bool keepAspectRatio, vec4 color, bool isDepthTexture)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Billboard.");
	
	float width = dimensions.x;
	float height = dimensions.y;
	if (keepAspectRatio)
	{
		D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();
		float aspect = (float)desc.Width / desc.Height;

		width = height * aspect;
		if (width > dimensions.x)
		{
			width = dimensions.x;
			height = width / aspect;
		}
	}

	vec3 right = camera.rotation * vec3(width / 2, 0.f, 0.f);
	vec3 up = camera.rotation * vec3(0.f, height / 2, 0.f);

	vertex_3PU vertices[] =
	{
		{ position - right - up, vec2(0.f, 0.f) },
		{ position + right - up, vec2(1.f, 0.f) },
		{ position - right + up, vec2(0.f, 1.f) },
		{ position + right + up, vec2(1.f, 1.f) },
	};

	D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(vertices, arraysize(vertices));


	commandList->setPipelineState(unlitTexturedPipelineState);
	commandList->setGraphicsRootSignature(unlitTexturedRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	commandList->setVertexBuffer(0, tmpVertexBuffer);

	struct
	{
		mat4 vp;
		vec4 color;
	} cb =
	{
		camera.viewProjectionMatrix,
		color
	};

	commandList->setGraphics32BitConstants(DEBUG_DISPLAY_ROOTPARAM_CB, cb);

	if (isDepthTexture)
	{
		commandList->bindDepthTextureForReading(DEBUG_DISPLAY_ROOTPARAM_TEXTURE, 0, texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	else
	{
		commandList->setShaderResourceView(DEBUG_DISPLAY_ROOTPARAM_TEXTURE, 0, texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	commandList->draw(arraysize(vertices), 1, 0, 0);
}

void debug_display::renderLineMesh(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh, mat4 transform)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Line mesh.");

	commandList->setPipelineState(unlitLinePipelineState);
	commandList->setGraphicsRootSignature(unlitFlatRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	struct
	{
		mat4 vp;
		vec4 color;
	} cb =
	{
		camera.viewProjectionMatrix * transform,
		vec4(1.f, 1.f, 1.f, 1.f)
	};
	commandList->setGraphics32BitConstants(DEBUG_DISPLAY_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);

	commandList->drawIndexed(mesh.indexBuffer.numIndices, 1, 0, 0, 0);
}

void debug_display::renderLineStrip(dx_command_list* commandList, const render_camera& camera, vec3* vertices, uint32 numVertices, vec4 color)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Line mesh.");

	commandList->setPipelineState(unlitLinePipelineState);
	commandList->setGraphicsRootSignature(unlitFlatRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(vertices, numVertices);

	struct
	{
		mat4 vp;
		vec4 color;
	} cb =
	{
		camera.viewProjectionMatrix,
		color
	};
	commandList->setGraphics32BitConstants(DEBUG_DISPLAY_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, tmpVertexBuffer);

	commandList->draw(numVertices, 1, 0, 0);
}

void debug_display::renderLine(dx_command_list* commandList, const render_camera& camera, vec3 from, vec3 to, vec4 color)
{
	PROFILE_FUNCTION();

	vec3 vertices[] = { from, to, };
	renderLineStrip(commandList, camera, vertices, 2, color);
}

void debug_display::renderFrustum(dx_command_list* commandList, const render_camera& camera, const camera_frustum_corners& frustum, vec4 color)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Line mesh.");

	commandList->setPipelineState(unlitLinePipelineState);
	commandList->setGraphicsRootSignature(unlitFlatRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);


	D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(frustum.corners, arraysize(frustum.corners));

	struct
	{
		mat4 vp;
		vec4 color;
	} cb =
	{
		camera.viewProjectionMatrix,
		vec4(1.f, 1.f, 1.f, 1.f)
	};
	commandList->setGraphics32BitConstants(DEBUG_DISPLAY_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, tmpVertexBuffer);
	commandList->setIndexBuffer(frustumIndexBuffer);

	commandList->drawIndexed(frustumIndexBuffer.numIndices, 1, 0, 0, 0);
}
