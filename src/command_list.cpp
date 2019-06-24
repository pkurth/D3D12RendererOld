#include "command_list.h"
#include "error.h"

#include <dx/d3dx12.h>

void dx_command_list::initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType)
{
	this->device = device;
	this->commandListType = commandListType;
	checkResult(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));
	checkResult(device->CreateCommandList(0, commandListType, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	checkResult(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));
}

void dx_command_list::transitionResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		resource.Get(),
		beforeState, afterState);

	commandList->ResourceBarrier(1, &barrier);
}

void dx_command_list::clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void dx_command_list::clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void dx_command_list::updateBufferResource(ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numElements * elementSize;

	// Create a committed resource for the GPU resource in a default heap.
	checkResult(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(pDestinationResource)));

	// Create an committed resource for the upload.
	if (bufferData)
	{
		checkResult(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIntermediateResource)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(commandList.Get(),
			*pDestinationResource, *pIntermediateResource,
			0, 0, 1, &subresourceData);
	}
}

void dx_command_list::reset()
{
	checkResult(commandAllocator->Reset());
	checkResult(commandList->Reset(commandAllocator.Get(), nullptr));
}

void dx_command_list::close()
{
	checkResult(commandList->Close());
}
