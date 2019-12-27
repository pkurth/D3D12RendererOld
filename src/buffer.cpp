#include "pch.h"
#include "buffer.h"
#include "error.h"
#include "resource_state_tracker.h"
#include "command_list.h"
#include "command_queue.h"

void dx_buffer::initialize(ComPtr<ID3D12Device2> device, uint32 size, const void* data, dx_command_list* commandList,
	D3D12_RESOURCE_FLAGS flags)
{
	this->device = device;

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

void dx_buffer::copyBackToCPU(void* buffer, uint32 size)
{
	D3D12_RESOURCE_DESC readbackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
	ComPtr<ID3D12Resource> readbackBuffer;
	checkResult(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&readbackBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackBuffer)));

	dx_command_list* commandList = dx_command_queue::copyCommandQueue.getAvailableCommandList();

	commandList->copyResource(readbackBuffer, resource, false);

	uint64 fenceValue = dx_command_queue::copyCommandQueue.executeCommandList(commandList);
	dx_command_queue::copyCommandQueue.waitForFenceValue(fenceValue);

	D3D12_RANGE readbackBufferRange{ 0, size };
	void* data;
	checkResult(readbackBuffer->Map(0, &readbackBufferRange, &data));

	memcpy(buffer, data, size);

	D3D12_RANGE emptyRange{ 0, 0 };
	readbackBuffer->Unmap(0, &emptyRange);
}

void dx_structured_buffer::initialize(ComPtr<ID3D12Device2> device, uint32 count, uint32 elementSize, const void* data, dx_command_list* commandList)
{
	uint32 size = count * elementSize;

	dx_buffer::initialize(device, size, data, commandList, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	this->count = count;
	this->elementSize = elementSize;

	srv = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).getDescriptorHandle(0);
	createShaderResourceView(device, srv);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = count;
	uavDesc.Buffer.StructureByteStride = elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	uav = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).getDescriptorHandle(0);
	device->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, uav);
}

void dx_structured_buffer::createShaderResourceView(ComPtr<ID3D12Device2> device, D3D12_CPU_DESCRIPTOR_HANDLE srv)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = count;
	srvDesc.Buffer.StructureByteStride = elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	device->CreateShaderResourceView(resource.Get(), &srvDesc, srv);
}
