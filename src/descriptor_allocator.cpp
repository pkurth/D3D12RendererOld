#include "descriptor_allocator.h"
#include "error.h"

dx_descriptor_allocator dx_descriptor_allocator::allocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

void dx_descriptor_allocator::initializeInternal(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptorsPerHeap)
{
	this->device = device;
	this->type = type;
	this->numDescriptorsPerHeap = numDescriptorsPerHeap;
}

dx_descriptor_allocation dx_descriptor_allocator::allocateDescriptorsInternal(uint32 count)
{
	std::lock_guard<std::mutex> lock(allocationMutex);

	dx_descriptor_allocation allocation;

	for (auto iter = freePages.begin(); iter != freePages.end(); ++iter)
	{
		dx_descriptor_allocator_page& page = **iter;

		allocation = page.allocateDescriptors(count);

		if (page.numFreeHandles == 0)
		{
			iter = freePages.erase(iter);
		}

		// A valid allocation has been found.
		if (!allocation.isNull())
		{
			break;
		}
	}

	// No available heap could satisfy the requested number of descriptors.
	if (allocation.isNull())
	{
		numDescriptorsPerHeap = max(numDescriptorsPerHeap, count);
		dx_descriptor_allocator_page& newPage = createPage();

		allocation = newPage.allocateDescriptors(count);
	}

	return allocation;
}

void dx_descriptor_allocator::releaseStaleDescriptorsInternal(uint64 frameNumber)
{
	std::lock_guard<std::mutex> lock(allocationMutex);

	for (size_t i = 0; i < pages.size(); ++i)
	{
		dx_descriptor_allocator_page& page = pages[i];

		page.releaseStaleDescriptors(frameNumber);

		if (page.numFreeHandles > 0)
		{
			freePages.insert(&page);
		}
	}
}

dx_descriptor_allocator_page& dx_descriptor_allocator::createPage()
{
	pages.emplace_back(device, type, numDescriptorsPerHeap);
	dx_descriptor_allocator_page& result = pages.back();
	freePages.insert(&result);
	return result;
}

dx_descriptor_allocator_page::dx_descriptor_allocator_page(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors)
{
	this->type = type;
	this->numFreeHandles = numDescriptors;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = type;
	heapDesc.NumDescriptors = numDescriptors;

	checkResult(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));

	baseDescriptor = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(type);

	// Initialize the free lists
	//AddNewBlock(0, m_NumFreeHandles);


	currentOffset = 0;
}

dx_descriptor_allocation dx_descriptor_allocator_page::allocateDescriptors(uint32 count)
{
	numFreeHandles -= count;
	uint32 offset = currentOffset;
	currentOffset += count;

	return dx_descriptor_allocation(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(baseDescriptor, offset, descriptorHandleIncrementSize),
		count, descriptorHandleIncrementSize, this);
}

void dx_descriptor_allocator_page::releaseStaleDescriptors(uint64 frameNumber)
{
}

dx_descriptor_allocation::dx_descriptor_allocation()
	: baseHandle()
	, count(0)
	, descriptorSize(0)
	, page(nullptr)
{
}

dx_descriptor_allocation::dx_descriptor_allocation(CD3DX12_CPU_DESCRIPTOR_HANDLE baseHandle, uint32 count, uint32 descriptorSize, dx_descriptor_allocator_page* page)
	: baseHandle(baseHandle)
	, count(count)
	, descriptorSize(descriptorSize)
	, page(page)
{
}
