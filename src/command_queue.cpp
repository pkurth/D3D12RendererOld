#include "command_queue.h"
#include "error.h"
#include "resource_state_tracker.h"

#include <cassert>

void dx_command_queue::initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	fenceValue = 0;
	commandListType = type;
	this->device = device;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	checkResult(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
	checkResult(device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

	processInFlightCommandListsThread = std::thread(&dx_command_queue::processInFlightCommandLists, this);
}

dx_command_queue::~dx_command_queue()
{
	continueProcessingInFlightCommandLists = false;
	processInFlightCommandListsThread.join();

	dx_command_list* list;
	while (commandLists.tryGetBack(list))
	{
		delete list;
	}
}

dx_command_list* dx_command_queue::getAvailableCommandList()
{
	dx_command_list* result;

	if (!freeCommandLists.tryPop(result))
	{
		result = new dx_command_list;
		result->initialize(device, commandListType);
		commandLists.pushBack(result);
	}

	return result;
}

uint64 dx_command_queue::executeCommandList(dx_command_list* commandList)
{
	return executeCommandLists({ commandList });
}

uint64 dx_command_queue::executeCommandLists(const std::vector<dx_command_list*>& commandLists)
{
	dx_resource_state_tracker::lock();

	std::vector<dx_command_list*> toBeQueued;
	toBeQueued.reserve(commandLists.size() * 2);

	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(commandLists.size() * 2);

	for (dx_command_list* list : commandLists)
	{
		dx_command_list* pendingCommandList = getAvailableCommandList();
		bool hasPendingBarriers = list->close(pendingCommandList);
		pendingCommandList->close();

		if (hasPendingBarriers)
		{
			d3d12CommandLists.push_back(pendingCommandList->getD3D12CommandList().Get());
		}
		d3d12CommandLists.push_back(list->getD3D12CommandList().Get());

		toBeQueued.push_back(pendingCommandList);
		toBeQueued.push_back(list);
	}

	uint32 numCommandLists = (uint32)d3d12CommandLists.size();
	commandQueue->ExecuteCommandLists(numCommandLists, d3d12CommandLists.data());
	uint64 fenceValue = signal();

	dx_resource_state_tracker::unlock();

	for (auto commandList : toBeQueued)
	{
		inFlightCommandLists.pushBack(command_list_entry{ fenceValue, commandList });
	}

	return fenceValue;
}

uint64 dx_command_queue::signal()
{
	uint64 fenceValueForSignal = ++fenceValue;
	checkResult(commandQueue->Signal(fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

bool dx_command_queue::isFenceComplete(uint64 fenceValue)
{
	return fence->GetCompletedValue() >= fenceValue;
}

void dx_command_queue::waitForFenceValue(uint64 fenceValue)
{
	if (!isFenceComplete(fenceValue))
	{
		HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fenceEvent && "Failed to create fence event handle.");

		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, DWORD_MAX);

		CloseHandle(fenceEvent);
	}
}

void dx_command_queue::flush()
{
	std::unique_lock<std::mutex> lock(inFlightCommandListsMutex);

	struct wait_condition
	{
		bool operator()()
		{
			return inFlightCommandLists.empty();
		}

		thread_safe_queue<command_list_entry>& inFlightCommandLists;
	};

	processInFlightCommandListsCondition.wait(lock, wait_condition{ inFlightCommandLists });
	waitForFenceValue(fenceValue);
}

ComPtr<ID3D12CommandQueue> dx_command_queue::getD3D12CommandQueue() const
{
	return commandQueue;
}

void dx_command_queue::processInFlightCommandLists()
{
	std::unique_lock<std::mutex> lock(inFlightCommandListsMutex, std::defer_lock);

	while (continueProcessingInFlightCommandLists)
	{
		command_list_entry commandListEntry;

		lock.lock();
		while (inFlightCommandLists.tryPop(commandListEntry))
		{
			uint64 fenceValue = commandListEntry.fenceValue;
			dx_command_list* commandList = commandListEntry.commandList;

			waitForFenceValue(fenceValue);

			commandList->reset();

			freeCommandLists.pushBack(commandList);
		}
		lock.unlock();
		processInFlightCommandListsCondition.notify_one();

		std::this_thread::yield();
	}
}
