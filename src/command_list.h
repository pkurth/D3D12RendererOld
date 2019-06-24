#pragma once

#include "common.h"

#include <d3d12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

class dx_command_list
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType);

	void transitionResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	void clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);

	void clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

	void updateBufferResource(ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
		size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	void reset();
	void close();

	inline ComPtr<ID3D12GraphicsCommandList2> getD3D12CommandList() const { return commandList; }

private:
	D3D12_COMMAND_LIST_TYPE				commandListType;
	ComPtr<ID3D12Device2>				device;
	ComPtr<ID3D12CommandAllocator>		commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2>	commandList;
};
