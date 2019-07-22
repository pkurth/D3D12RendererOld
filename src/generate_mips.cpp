#include "pch.h"
#include "generate_mips.h"
#include "error.h"
#include "descriptor_allocator.h"


void dx_generate_mips_pso::initialize(ComPtr<ID3D12Device2> device)
{
	CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[generate_mips_num_params];
	rootParameters[generate_mips_param_constant_buffer].InitAsConstants(sizeof(generate_mips_cb) / 4, 0);
	rootParameters[generate_mips_param_src].InitAsDescriptorTable(1, &srcMip);
	rootParameters[generate_mips_param_out].InitAsDescriptorTable(1, &outMip);

	CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &linearClampSampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignature.initialize(device, rootSignatureDesc);


	ComPtr<ID3DBlob> shaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/generate_mips.cso", &shaderBlob));

	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS cs;
	} pipelineStateStream;

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.cs = CD3DX12_SHADER_BYTECODE(shaderBlob.Get());

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));


	dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);
	defaultUAV = allocation.getDescriptorHandle(0);

	for (uint32 i = 0; i < 4; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture2D.MipSlice = i;
		uavDesc.Texture2D.PlaneSlice = 0;

		device->CreateUnorderedAccessView(
			nullptr, nullptr, &uavDesc,
			allocation.getDescriptorHandle(i)
		);
	}
}
