#include "pch.h"
#include "lighting.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void visualize_light_probe_pipeline::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget)
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/visualize_light_probe_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/visualize_light_probe_ps.cso", &pixelShaderBlob));

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
	rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_CB].InitAsConstants(sizeof(mat4) / sizeof(float) + 1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // VP matrix.
	rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

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

	SET_NAME(rootSignature.rootSignature, "Visualize Light Probe Root Signature");
	SET_NAME(pipelineState, "Visualize Light Probe Pipeline");


	cpu_mesh<vertex_3P> cube;
	cube.pushSphere(51, 51, 1.f);
	mesh.initialize(device, commandList, cube);
}

void visualize_light_probe_pipeline::render(dx_command_list* commandList, const render_camera& camera, vec3 position, dx_texture& cubemap,
	float uvzScale)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize light probe.");

	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(rootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	struct
	{
		mat4 mvp;
		float uvzScale;
	} cb = {
		camera.projectionMatrix * camera.viewMatrix * createModelMatrix(position, quat::identity),
		uvzScale
	};
	commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);
	commandList->bindCubemap(VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE, 0, cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->drawIndexed(mesh.indexBuffer.numIndices, 1, 0, 0, 0);
}
