#include "pch.h"
#include "command_queue.h"
#include "error.h"
#include "resource_state_tracker.h"
#include "profiling.h"


dx_command_queue dx_command_queue::renderCommandQueue;
dx_command_queue dx_command_queue::computeCommandQueue;
dx_command_queue dx_command_queue::copyCommandQueue;

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
	PROFILE_FUNCTION();

	dx_command_list* result;

	if (!freeCommandLists.tryPop(result))
	{
		result = new dx_command_list;
		result->initialize(device, commandListType);
		commandLists.pushBack(result);
	}

	return result;
}

dx_command_queue::dx_transition_command_list* dx_command_queue::getAvailableTransitionCommandList()
{
	dx_transition_command_list* result;

	if (!freeTransitionCommandLists.tryPop(result))
	{
		result = new dx_transition_command_list;
		result->initialize(device, commandListType);
		transitionCommandLists.pushBack(result);
	}

	return result;
}

uint64 dx_command_queue::executeCommandList(dx_command_list* commandList)
{
	return executeCommandLists(&commandList, 1);
}

uint64 dx_command_queue::executeCommandLists(dx_command_list** commandLists, uint32 numCommandLists)
{
	PROFILE_FUNCTION();

	dx_resource_state_tracker::lock();

	command_list_entry toBeQueued[128];
	uint32 numToBeQueued = 0;

	ID3D12CommandList* d3d12CommandLists[128];
	uint32 numD3D12CommandLists = 0;

	dx_command_list* extraComputeCommandLists[128];
	uint32 numExtraComputeCommandLists = 0;

	{
		PROFILE_BLOCK("Gather lists to execute");

		for (uint32 i = 0; i < numCommandLists; ++i)
		{
			dx_command_list* list = commandLists[i];

			dx_transition_command_list* pendingCommandList = getAvailableTransitionCommandList();
			bool hasPendingBarriers = list->close(pendingCommandList->commandList);

			if (hasPendingBarriers)
			{
				checkResult(pendingCommandList->commandList->Close());
				d3d12CommandLists[numD3D12CommandLists++] = pendingCommandList->commandList.Get();
				toBeQueued[numToBeQueued++] = pendingCommandList;
			}
			else
			{
				checkResult(pendingCommandList->commandAllocator->Reset());
				checkResult(pendingCommandList->commandList->Reset(pendingCommandList->commandAllocator.Get(), nullptr));
				freeTransitionCommandLists.pushBack(pendingCommandList);
			}

			d3d12CommandLists[numD3D12CommandLists++] = list->getD3D12CommandList().Get();
			toBeQueued[numToBeQueued++] = list;

			dx_command_list* extraComputeCommandList = list->getComputeCommandList();
			if (extraComputeCommandList)
			{
				extraComputeCommandLists[numExtraComputeCommandLists++] = extraComputeCommandList;
			}
		}
	}

	{
		PROFILE_BLOCK("Execute");
		commandQueue->ExecuteCommandLists(numD3D12CommandLists, d3d12CommandLists);
	}
	uint64 fenceValue = signal();

	dx_resource_state_tracker::unlock();

	for (uint32 i = 0; i < numToBeQueued; ++i)
	{
		toBeQueued[i].fenceValue = fenceValue;
		inFlightCommandLists.pushBack(toBeQueued[i]);
	}

	if (numExtraComputeCommandLists)
	{
		PROFILE_BLOCK("Execute extra compute lists");

		computeCommandQueue.waitForOtherQueue(*this);
		computeCommandQueue.executeCommandLists(extraComputeCommandLists, numExtraComputeCommandLists);
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
	waitForFenceValue(signal());
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

			waitForFenceValue(fenceValue);

			if (commandListEntry.isTransition)
			{
				dx_transition_command_list* commandList = commandListEntry.transition;
				checkResult(commandList->commandAllocator->Reset());
				checkResult(commandList->commandList->Reset(commandList->commandAllocator.Get(), nullptr));
				freeTransitionCommandLists.pushBack(commandList);
			}
			else
			{
				dx_command_list* commandList = commandListEntry.commandList;
				commandList->reset();
				freeCommandLists.pushBack(commandList);
			}
		}

		lock.unlock();
		processInFlightCommandListsCondition.notify_one();

		std::this_thread::yield();
	}
}

void dx_command_queue::waitForOtherQueue(dx_command_queue& other)
{
	commandQueue->Wait(other.fence.Get(), other.signal());
}

void dx_command_queue::dx_transition_command_list::initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType)
{
	checkResult(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));
	checkResult(device->CreateCommandList(0, commandListType, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	checkResult(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));
}
