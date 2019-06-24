#pragma once

#include "common.h"
#include "command_list.h"
#include "thread_safe_queue.h"
#include "thread_safe_vector.h"

#include <d3d12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

#include <mutex>
#include <atomic>

class dx_command_queue
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	virtual ~dx_command_queue();

	dx_command_list* getAvailableCommandList();

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64 executeCommandList(dx_command_list* commandList);

	bool isFenceComplete(uint64 fenceValue);
	void waitForFenceValue(uint64 fenceValue);
	void flush();

	ComPtr<ID3D12CommandQueue> getD3D12CommandQueue() const;

protected:
	uint64 signal();
	void processInFlightCommandLists();

private:

	D3D12_COMMAND_LIST_TYPE                     commandListType;
	ComPtr<ID3D12Device2>						device;
	ComPtr<ID3D12CommandQueue>					commandQueue;
	ComPtr<ID3D12Fence>							fence;
	std::atomic_uint64_t	                    fenceValue;

	struct command_list_entry
	{
		uint64				fenceValue;
		dx_command_list*	commandList;
	};

	thread_safe_vector<dx_command_list*>		commandLists;
	thread_safe_queue<dx_command_list*>			freeCommandLists;
	thread_safe_queue<command_list_entry>		inFlightCommandLists;

	bool										continueProcessingInFlightCommandLists = true;
	std::mutex									inFlightCommandListsMutex;
	std::condition_variable						processInFlightCommandListsCondition;
	std::thread									processInFlightCommandListsThread;
};

