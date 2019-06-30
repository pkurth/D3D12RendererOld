#pragma once

#include "common.h"

#include <dx/d3dx12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

#include <vector>
#include <set>
#include <map>
#include <mutex>

struct dx_descriptor_allocator_page;

struct dx_descriptor_allocation
{
	dx_descriptor_allocation();
	dx_descriptor_allocation(CD3DX12_CPU_DESCRIPTOR_HANDLE baseHandle, uint32 count, uint32 descriptorSize, dx_descriptor_allocator_page* page);

	inline bool isNull() const { return baseHandle.ptr == 0; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE getDescriptorHandle(uint32 i) const { assert(i < count); return { baseHandle.ptr + (descriptorSize * i) }; }

	CD3DX12_CPU_DESCRIPTOR_HANDLE baseHandle;
	uint32 count;
	uint32 descriptorSize;
	dx_descriptor_allocator_page* page;
};

struct dx_descriptor_allocator_page
{
	dx_descriptor_allocator_page(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors);

	dx_descriptor_allocation allocateDescriptors(uint32 count);
	void releaseStaleDescriptors(uint64 frameNumber);


	uint32 numFreeHandles;
	D3D12_DESCRIPTOR_HEAP_TYPE type;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE baseDescriptor;
	uint32 descriptorHandleIncrementSize;


	// tmp
	uint32 currentOffset;
};

class dx_descriptor_allocator
{
public:

	static void initialize(ComPtr<ID3D12Device2> device, uint32 numDescriptorsPerHeap = 1024)
	{
		for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			allocators[i].initializeInternal(device, (D3D12_DESCRIPTOR_HEAP_TYPE)i, numDescriptorsPerHeap);
		}
	}

	static dx_descriptor_allocation allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 count = 1)
	{
		return allocators[type].allocateDescriptorsInternal(count);
	}

	static void releaseStaleDescriptors(uint64 frameNumber)
	{
		for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			allocators[i].releaseStaleDescriptorsInternal(frameNumber);
		}
	}

private:
	void initializeInternal(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptorsPerHeap);
	dx_descriptor_allocation allocateDescriptorsInternal(uint32 count);
	void releaseStaleDescriptorsInternal(uint64 frameNumber);
	dx_descriptor_allocator_page& createPage();


	uint32 numDescriptorsPerHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE type;

	std::vector<dx_descriptor_allocator_page> pages;
	std::set<dx_descriptor_allocator_page*> freePages;

	ComPtr<ID3D12Device2> device;

	std::mutex allocationMutex;

	static dx_descriptor_allocator allocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};
