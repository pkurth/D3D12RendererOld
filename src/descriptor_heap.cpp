#include "pch.h"
#include "descriptor_heap.h"
#include "error.h"

void dx_descriptor_heap::initialize(ComPtr<ID3D12Device2> device, uint32 numDescriptors)
{
	this->device = device;

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	checkResult(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	cpuHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_descriptor_heap::push2DTexture(dx_texture& texture)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = gpuHandle;

	device->CreateShaderResourceView(texture.resource.Get(), nullptr, cpuHandle);
	cpuHandle.Offset(descriptorHandleIncrementSize);
	gpuHandle.Offset(descriptorHandleIncrementSize);

	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_descriptor_heap::pushCubemap(dx_texture& texture)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = gpuHandle;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texture.resource->GetDesc().Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = (uint32)-1; // Use all mips.

	device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);
	cpuHandle.Offset(descriptorHandleIncrementSize);
	gpuHandle.Offset(descriptorHandleIncrementSize);

	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_descriptor_heap::pushDepthTexture(dx_texture& texture)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = gpuHandle;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = dx_texture::getReadFormatFromTypeless(texture.resource->GetDesc().Format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);
	cpuHandle.Offset(descriptorHandleIncrementSize);
	gpuHandle.Offset(descriptorHandleIncrementSize);

	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_descriptor_heap::pushNullTexture()
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = gpuHandle;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 0;

	device->CreateShaderResourceView(nullptr, &srvDesc, cpuHandle);
	cpuHandle.Offset(descriptorHandleIncrementSize);
	gpuHandle.Offset(descriptorHandleIncrementSize);

	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_descriptor_heap::pushStructuredBuffer(dx_structured_buffer& buffer)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE result = gpuHandle;

	buffer.createShaderResourceView(device, cpuHandle);
	cpuHandle.Offset(descriptorHandleIncrementSize);
	gpuHandle.Offset(descriptorHandleIncrementSize);

	return result;
}
