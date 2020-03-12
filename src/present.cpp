#include "pch.h"
#include "present.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void present_pipeline::initialize(ComPtr<ID3D12Device2> device, const D3D12_RT_FORMAT_ARRAY& renderTargetFormat)
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/fullscreen_triangle_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/present_ps.cso", &pixelShaderBlob));

	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[4];
	rootParameters[PRESENT_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Camera.
	rootParameters[PRESENT_ROOTPARAM_MODE].InitAsConstants(2, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Present params.
	rootParameters[PRESENT_ROOTPARAM_TONEMAP].InitAsConstants(8, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Tonemap params.
	rootParameters[PRESENT_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL); // Texture.

	CD3DX12_STATIC_SAMPLER_DESC sampler(0,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

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
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
	} pipelineStateStream;

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { nullptr, 0 };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.rtvFormats = renderTargetFormat;

	CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
	depthDesc.DepthEnable = false; // Don't do depth-check.
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
	pipelineStateStream.depthStencilDesc = depthDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

	SET_NAME(rootSignature.rootSignature, "Present Root Signature");
	SET_NAME(pipelineState, "Present Pipeline");


	tonemapParams.exposure = 0.2f;
	tonemapParams.A = 0.22f;
	tonemapParams.B = 0.3f;
	tonemapParams.C = 0.1f;
	tonemapParams.D = 0.2f;
	tonemapParams.E = 0.01f;
	tonemapParams.F = 0.3f;
	tonemapParams.linearWhite = 11.2f;
}

void present_pipeline::render(dx_command_list* commandList, dx_texture& hdrTexture)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Tonemap and present.");

	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(rootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	struct present_cb
	{
		uint32 displayMode;
		float standardNits;
	} presentCB =
	{
		0, 0.f
	};

	commandList->setGraphics32BitConstants(PRESENT_ROOTPARAM_MODE, presentCB);
	commandList->setGraphics32BitConstants(PRESENT_ROOTPARAM_TONEMAP, tonemapParams);

	commandList->setShaderResourceView(PRESENT_ROOTPARAM_TEXTURE, 0, hdrTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->draw(3, 1, 0, 0);
}
