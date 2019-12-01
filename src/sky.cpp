#include "pch.h"
#include "sky.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void sky_pipeline::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget)
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/sky_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/sky_ps.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[SKY_ROOTPARAM_VP].InitAsConstants(sizeof(mat4) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // VP matrix.
	rootParameters[SKY_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

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

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
	pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
	pipelineStateStream.rasterizer = defaultRasterizerDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

	SET_NAME(rootSignature.rootSignature, "Sky Root Signature");
	SET_NAME(pipelineState, "Sky Pipeline");


	cpu_triangle_mesh<vertex_3P> skybox;
	skybox.pushCube(1.f, true);
	mesh.initialize(device, commandList, skybox);
}

void sky_pipeline::render(dx_command_list* commandList, const render_camera& camera, dx_texture& cubemap)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Sky.");

	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(rootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	mat4 view = camera.viewMatrix;
	view.m03 = 0.f; view.m13 = 0.f; view.m23 = 0.f;
	mat4 skyVP = camera.projectionMatrix * view;

	commandList->setGraphics32BitConstants(SKY_ROOTPARAM_VP, skyVP);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);
	commandList->bindCubemap(SKY_ROOTPARAM_TEXTURE, 0, cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->drawIndexed(mesh.indexBuffer.numIndices, 1, 0, 0, 0);
}
