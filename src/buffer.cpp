#include "pch.h"
#include "buffer.h"
#include "error.h"
#include "resource_state_tracker.h"
#include "command_list.h"

void dx_buffer::initialize(ComPtr<ID3D12Device2> device, uint32 size, const void* data, dx_command_list* commandList,
	D3D12_RESOURCE_FLAGS flags)
{
	// Create a committed resource for the GPU resource in a default heap.
	checkResult(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size, flags),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&resource)));

	dx_resource_state_tracker::addGlobalResourceState(resource.Get(), D3D12_RESOURCE_STATE_COMMON, 1);

	if (data)
	{
		assert(commandList);
		commandList->uploadBufferData(resource, data, size);
	}
}

void dx_structured_buffer::initialize(ComPtr<ID3D12Device2> device, uint32 count, uint32 elementSize, const void* data, dx_command_list* commandList)
{
	uint32 size = count * elementSize;

	dx_buffer::initialize(device, size, data, commandList, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = count;
	srvDesc.Buffer.StructureByteStride = elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	srv = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).getDescriptorHandle(0);
	device->CreateShaderResourceView(resource.Get(), &srvDesc, srv);
}
