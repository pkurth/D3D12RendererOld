#pragma once

#include "common.h"
#include "commands.h"

#include <memory>
#include <deque>
#include <vector>

class dx_upload_buffer
{
public:
	struct allocation
	{
		void* cpu;
		D3D12_GPU_VIRTUAL_ADDRESS gpu;
	};

	void initialize(ComPtr<ID3D12Device2> device, uint64 pageSize = MB(2));

	/**
	 * Allocate memory in an Upload heap.
	 * An allocation must not exceed the size of a page.
	 * Use a memcpy or similar method to copy the
	 * buffer data to CPU pointer in the Allocation structure returned from
	 * this function.
	 */
	allocation allocate(uint64 sizeInBytes, uint64 alignment);

	/**
	 * Release all allocated pages. This should only be done when the command list
	 * is finished executing on the CommandQueue.
	 */
	void reset();

private:
	// A single page for the allocator.
	struct memory_page
	{
		memory_page(ComPtr<ID3D12Device2> device, uint64 sizeInBytes);
		~memory_page();

		bool hasSpace(uint64 sizeInBytes, uint64 alignment) const;
		allocation allocate(uint64 sizeInBytes, uint64 alignment);
		void reset();

		ComPtr<ID3D12Resource>		resource;

		void*						cpuBasePtr;
		D3D12_GPU_VIRTUAL_ADDRESS	gpuBasePtr;

		uint64						pageSize;
		uint64						currentOffset;
	};


	memory_page* requestPage();

	std::vector<memory_page>		pages;
	std::vector<memory_page*>		freePages;
	memory_page*					currentPage;

	uint64							pageSize;
	ComPtr<ID3D12Device2>			device;
};
