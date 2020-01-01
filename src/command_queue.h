#pragma once

#include "common.h"
#include "command_list.h"
#include "thread_safe_queue.h"
#include "thread_safe_vector.h"


class dx_command_queue
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	virtual ~dx_command_queue();

	dx_command_queue() = default;

	dx_command_queue(const dx_command_queue&) = delete;
	dx_command_queue(dx_command_queue&&) = delete;
	dx_command_queue& operator=(const dx_command_queue&) = delete;
	dx_command_queue& operator=(dx_command_queue&&) = delete;

	dx_command_list* getAvailableCommandList();

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64 executeCommandList(dx_command_list* commandList);
	uint64 executeCommandLists(const std::vector<dx_command_list*>& commandLists);

	bool isFenceComplete(uint64 fenceValue);
	void waitForFenceValue(uint64 fenceValue);
	void waitForOtherQueue(dx_command_queue& other);
	
	void flush();

	ComPtr<ID3D12CommandQueue> getD3D12CommandQueue() const;



	static dx_command_queue						renderCommandQueue;
	static dx_command_queue						computeCommandQueue;
	static dx_command_queue						copyCommandQueue;

protected:
	uint64 signal();
	void processInFlightCommandLists();

private:
	
	struct dx_transition_command_list
	{
		void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType);

		ComPtr<ID3D12CommandAllocator>		commandAllocator;
		ComPtr<ID3D12GraphicsCommandList2>	commandList;
	};

	dx_transition_command_list* getAvailableTransitionCommandList();

	D3D12_COMMAND_LIST_TYPE                     commandListType;
	ComPtr<ID3D12Device2>						device;
	ComPtr<ID3D12CommandQueue>					commandQueue;
	ComPtr<ID3D12Fence>							fence;
	std::atomic_uint64_t	                    fenceValue;

	struct command_list_entry
	{
		uint64				fenceValue;

		union
		{
			dx_command_list* commandList;
			dx_transition_command_list* transition;
		};

		bool isTransition;

		command_list_entry() { }
		command_list_entry(dx_command_list* cl) { commandList = cl; isTransition = false; }
		command_list_entry(dx_transition_command_list* cl) { transition = cl; isTransition = true; }
	};

	thread_safe_vector<dx_command_list*>		commandLists;
	thread_safe_queue<dx_command_list*>			freeCommandLists;

	thread_safe_vector<dx_transition_command_list*>	transitionCommandLists;
	thread_safe_queue<dx_transition_command_list*>	freeTransitionCommandLists;

	thread_safe_queue<command_list_entry>		inFlightCommandLists;

	bool										continueProcessingInFlightCommandLists = true;
	std::mutex									inFlightCommandListsMutex;
	std::condition_variable						processInFlightCommandListsCondition;
	std::thread									processInFlightCommandListsThread;

};

