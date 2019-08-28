#include "pch.h"
#include "lighting.h"
#include "error.h"
#include "graphics.h"
#include "light.h"

#include <pix3.h>

void lighting_pipeline::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget)
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/fullscreen_triangle_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/lighting_ps.cso", &pixelShaderBlob));

	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6 + MAX_NUM_SUN_SHADOW_CASCADES, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[3];
	rootParameters[LIGHTING_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // Camera.
	rootParameters[LIGHTING_ROOTPARAM_TEXTURES].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[LIGHTING_ROOTPARAM_DIRECTIONAL].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC samplers[] =
	{
		staticPointClampSampler(0),
		staticLinearClampSampler(1),
		staticPointBorderComparisonSampler(2),
	};

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = arraysize(samplers);
	rootSignature.initialize(device, rootSignatureDesc);

	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
	} pipelineStateStream;

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { nullptr, 0 };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
	pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;

	CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = false; // Don't do depth-check.
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL; // Write where geometry is.
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP; // Don't modify stencil.
	depthStencilDesc.StencilWriteMask = 0;
	pipelineStateStream.depthStencilDesc = depthStencilDesc;

	pipelineStateStream.blend = additiveBlendDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

	SET_NAME(rootSignature.rootSignature, "Lighting Root Signature");
	SET_NAME(pipelineState, "Lighting Pipeline");

	commandList->integrateBRDF(brdf);

	dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_NUM_SUN_SHADOW_CASCADES);
	defaultSRV = allocation.getDescriptorHandle(0);

	for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 0;

		device->CreateShaderResourceView(
			nullptr, &srvDesc,
			allocation.getDescriptorHandle(i)
		);
	}
}

void lighting_pipeline::render(dx_command_list* commandList, 
	dx_texture irradiance, dx_texture prefilteredEnvironment,
	dx_texture& albedoAOTexture, dx_texture& normalRoughnessMetalnessTexture, dx_texture& depthTexture,
	dx_texture* sunShadowMapCascades, uint32 numSunShadowCascades,
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress)
{
	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Lighting pass.");

	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(rootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setGraphicsDynamicConstantBuffer(LIGHTING_ROOTPARAM_CAMERA, cameraCBAddress);

	commandList->bindCubemap(LIGHTING_ROOTPARAM_TEXTURES, 0, irradiance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->bindCubemap(LIGHTING_ROOTPARAM_TEXTURES, 1, prefilteredEnvironment, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->setShaderResourceView(LIGHTING_ROOTPARAM_TEXTURES, 2, brdf, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->setShaderResourceView(LIGHTING_ROOTPARAM_TEXTURES, 3, albedoAOTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->setShaderResourceView(LIGHTING_ROOTPARAM_TEXTURES, 4, normalRoughnessMetalnessTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->bindDepthTextureForReading(LIGHTING_ROOTPARAM_TEXTURES, 5, depthTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (uint32 i = 0; i < numSunShadowCascades; ++i)
	{
		commandList->bindDepthTextureForReading(LIGHTING_ROOTPARAM_TEXTURES, 6 + i, sunShadowMapCascades[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	commandList->stageDescriptors(LIGHTING_ROOTPARAM_TEXTURES, 6 + numSunShadowCascades, MAX_NUM_SUN_SHADOW_CASCADES - numSunShadowCascades, defaultSRV);

	commandList->setGraphicsDynamicConstantBuffer(LIGHTING_ROOTPARAM_DIRECTIONAL, sunCBAddress);

	commandList->draw(3, 1, 0, 0);
}
