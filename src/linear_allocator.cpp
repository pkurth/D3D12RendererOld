#include "upload_buffer.h"
#include "error.h"

#include <cassert>

void dx_upload_buffer::initialize(ComPtr<ID3D12Device2> device, uint64 pageSize)
{
	this->pageSize = pageSize;
	this->device = device;
}

dx_upload_buffer::allocation dx_upload_buffer::allocate(uint64 sizeInBytes, uint64 alignment)
{
	assert(sizeInBytes < pageSize);

	if (!currentPage || !currentPage->hasSpace(sizeInBytes, alignment))
	{
		currentPage = requestPage();
	}

	return currentPage->allocate(sizeInBytes, alignment);
}

void dx_upload_buffer::reset()
{
	currentPage = nullptr;
	freePages.clear();
	freePages.reserve(pages.size());

	for (memory_page& page : pages)
	{
		page.reset();
		freePages.push_back(&page);
	}
}

dx_upload_buffer::memory_page* dx_upload_buffer::requestPage()
{
	memory_page* page;

	if (!freePages.empty())
	{
		page = freePages.back();
		freePages.pop_back();
	}
	else
	{
		pages.push_back(memory_page(device, pageSize));
		page = &pages.back();
	}

	return page;
}

dx_upload_buffer::memory_page::memory_page(ComPtr<ID3D12Device2> device, uint64 sizeInBytes)
	: pageSize(sizeInBytes)
	, currentOffset(0)
	, cpuBasePtr(nullptr)
	, gpuBasePtr(D3D12_GPU_VIRTUAL_ADDRESS(0))
{
	checkResult(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(pageSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	));

	gpuBasePtr = resource->GetGPUVirtualAddress();
	resource->Map(0, nullptr, &cpuBasePtr);
}

dx_upload_buffer::memory_page::~memory_page()
{
	resource->Unmap(0, nullptr);
	cpuBasePtr = nullptr;
	gpuBasePtr = D3D12_GPU_VIRTUAL_ADDRESS(0);
}

bool dx_upload_buffer::memory_page::hasSpace(uint64 sizeInBytes, uint64 alignment) const
{
	size_t alignedSize = alignTo(sizeInBytes, alignment);
	size_t alignedOffset = alignTo(currentOffset, alignment);

	return alignedOffset + alignedSize <= pageSize;
}

dx_upload_buffer::allocation dx_upload_buffer::memory_page::allocate(uint64 sizeInBytes, uint64 alignment)
{
	assert(hasSpace(sizeInBytes, alignment));

	size_t alignedSize = alignTo(sizeInBytes, alignment);
	currentOffset = alignTo(currentOffset, alignment);

	allocation result;
	result.cpu = (uint8_t*)cpuBasePtr + currentOffset;
	result.gpu = gpuBasePtr + currentOffset;

	currentOffset += alignedSize;

	return result;
}

void dx_upload_buffer::memory_page::reset()
{
	currentOffset = 0;
}
