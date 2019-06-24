#include "command_queue.h"
#include "error.h"

#include <cassert>

void command_queue::initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
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

	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event handle.");
}

command_queue::~command_queue()
{
	CloseHandle(fenceEvent);
}

ComPtr<ID3D12GraphicsCommandList2> command_queue::getAvailableCommandList()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2> commandList;

	if (!commandAllocatorQueue.empty() && isFenceComplete(commandAllocatorQueue.front().fenceValue))
	{
		commandAllocator = commandAllocatorQueue.front().commandAllocator;
		commandAllocatorQueue.pop();

		checkResult(commandAllocator->Reset());
	}
	else
	{
		commandAllocator = createCommandAllocator();
	}

	if (!commandListQueue.empty())
	{
		commandList = commandListQueue.front();
		commandListQueue.pop();

		checkResult(commandList->Reset(commandAllocator.Get(), nullptr));
	}
	else
	{
		commandList = createCommandList(commandAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	checkResult(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}

uint64 command_queue::executeCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	commandList->Close();

	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	checkResult(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList* const ppCommandLists[] = {
		commandList.Get()
	};

	commandQueue->ExecuteCommandLists(1, ppCommandLists);
	uint64_t fenceValue = signal();

	commandAllocatorQueue.emplace(command_allocator_entry{ fenceValue, commandAllocator });
	commandListQueue.push(commandList);

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	commandAllocator->Release();

	return fenceValue;
}

uint64 command_queue::signal()
{
	uint64 fenceValueForSignal = ++fenceValue;
	checkResult(commandQueue->Signal(fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

bool command_queue::isFenceComplete(uint64 fenceValue)
{
	return fence->GetCompletedValue() >= fenceValue;
}

void command_queue::waitForFenceValue(uint64 fenceValue)
{
	if (!isFenceComplete(fenceValue))
	{
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, DWORD_MAX);
	}
}

void command_queue::flush()
{
	waitForFenceValue(signal());
}

ComPtr<ID3D12CommandQueue> command_queue::getD3D12CommandQueue() const
{
	return commandQueue;
}

ComPtr<ID3D12CommandAllocator> command_queue::createCommandAllocator()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	checkResult(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList2> command_queue::createCommandList(ComPtr<ID3D12CommandAllocator> allocator)
{
	ComPtr<ID3D12GraphicsCommandList2> commandList;
	checkResult(device->CreateCommandList(0, commandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	return commandList;
}
