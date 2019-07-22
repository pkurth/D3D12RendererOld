#include "pch.h"
#include "brdf.h"
#include "error.h"
#include "descriptor_allocator.h"


void dx_equirectangular_to_cubemap_pso::initialize(ComPtr<ID3D12Device2> device)
{
	CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[equirectangular_to_cubemap_num_params];
	rootParameters[equirectangular_to_cubemap_param_constant_buffer].InitAsConstants(3, 0);
	rootParameters[equirectangular_to_cubemap_param_src].InitAsDescriptorTable(1, &srcMip);
	rootParameters[equirectangular_to_cubemap_param_out].InitAsDescriptorTable(1, &outMip);

	CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &linearRepeatSampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignature.initialize(device, rootSignatureDesc);


	ComPtr<ID3DBlob> shaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/equirectangular_to_cubemap.cso", &shaderBlob));

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


	dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5);
	defaultUAV = allocation.getDescriptorHandle(0);

	for (uint32 i = 0; i < 5; ++i)
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

void dx_cubemap_to_irradiance_pso::initialize(ComPtr<ID3D12Device2> device)
{
	CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[cubemap_to_irradiance_num_params];
	rootParameters[cubemap_to_irradiance_param_constant_buffer].InitAsConstants(1, 0);
	rootParameters[cubemap_to_irradiance_param_src].InitAsDescriptorTable(1, &srcMip);
	rootParameters[cubemap_to_irradiance_param_out].InitAsDescriptorTable(1, &outMip);

	CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &linearRepeatSampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignature.initialize(device, rootSignatureDesc);


	ComPtr<ID3DBlob> shaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/cubemap_to_irradiance.cso", &shaderBlob));

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
}

void dx_prefilter_environment_pso::initialize(ComPtr<ID3D12Device2> device)
{
	CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[prefilter_environment_num_params];
	rootParameters[prefilter_environment_param_constant_buffer].InitAsConstants(4, 0);
	rootParameters[prefilter_environment_param_src].InitAsDescriptorTable(1, &srcMip);
	rootParameters[prefilter_environment_param_out].InitAsDescriptorTable(1, &outMip);

	CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &linearRepeatSampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignature.initialize(device, rootSignatureDesc);


	ComPtr<ID3DBlob> shaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/prefilter_environment.cso", &shaderBlob));

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


	dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5);
	defaultUAV = allocation.getDescriptorHandle(0);

	for (uint32 i = 0; i < 5; ++i)
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

void dx_integrate_brdf_pso::initialize(ComPtr<ID3D12Device2> device)
{
	CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[integrate_brdf_num_params];
	rootParameters[integrate_brdf_param_constant_buffer].InitAsConstants(1, 0);
	rootParameters[integrate_brdf_param_out].InitAsDescriptorTable(1, &outMip);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignature.initialize(device, rootSignatureDesc);


	ComPtr<ID3DBlob> shaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/integrate_brdf.cso", &shaderBlob));

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
}
