#pragma once

#include "common.h"

#include <d3d12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

#include <queue>

class dx_command_queue
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	virtual ~dx_command_queue();

	ComPtr<ID3D12GraphicsCommandList2> getAvailableCommandList();

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64 executeCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList);

	bool isFenceComplete(uint64 fenceValue);
	void waitForFenceValue(uint64 fenceValue);
	void flush();

	ComPtr<ID3D12CommandQueue> getD3D12CommandQueue() const;

protected:
	uint64 signal();
	ComPtr<ID3D12CommandAllocator> createCommandAllocator();
	ComPtr<ID3D12GraphicsCommandList2> createCommandList(ComPtr<ID3D12CommandAllocator> allocator);

private:
	// Keep track of command allocators that are "in-flight"
	struct command_allocator_entry
	{
		uint64									fenceValue;
		ComPtr<ID3D12CommandAllocator>			commandAllocator;
	};

	using command_allocator_queue = std::queue<command_allocator_entry>;
	using command_list_queue = std::queue<ComPtr<ID3D12GraphicsCommandList2> >;

	D3D12_COMMAND_LIST_TYPE                     commandListType;
	ComPtr<ID3D12Device2>						device;
	ComPtr<ID3D12CommandQueue>					commandQueue;
	ComPtr<ID3D12Fence>							fence;
	HANDLE                                      fenceEvent;
	uint64	                                    fenceValue;

	command_allocator_queue                     commandAllocatorQueue;
	command_list_queue                          commandListQueue;
};

