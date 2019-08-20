#include "pch.h"
#include "buffer.h"
#include "error.h"
#include "resource_state_tracker.h"
#include "command_list.h"

void dx_buffer::initialize(ComPtr<ID3D12Device2> device, uint32 size, const void* data, dx_command_list* commandList)
{
	// Create a committed resource for the GPU resource in a default heap.
	checkResult(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE),
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
