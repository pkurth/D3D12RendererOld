#include "pch.h"
#include "dynamic_descriptor_heap.h"
#include "command_list.h"
#include "root_signature.h"
#include "error.h"

void dx_dynamic_descriptor_heap::initialize(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32 numDescriptorsPerHeap)
{
	descriptorHeapType = heapType;
	this->numDescriptorsPerHeap = numDescriptorsPerHeap;
	descriptorTableBitMask = 0;
	staleDescriptorTableBitMask = 0;
	currentCPUDescriptorHandle = D3D12_DEFAULT;
	currentGPUDescriptorHandle = D3D12_DEFAULT;
	numFreeHandles = 0;
	this->device = device;

	descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(heapType);

	descriptorHandleCache.resize(numDescriptorsPerHeap);
}

void dx_dynamic_descriptor_heap::stageDescriptors(uint32 rootParameterIndex, uint32 offset, uint32 numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
	assert(numDescriptors <= numDescriptorsPerHeap && rootParameterIndex < maxDescriptorTables);

	descriptor_table_cache& cache = descriptorTableCache[rootParameterIndex];

	assert((offset + numDescriptors) <= cache.numDescriptors);

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (cache.baseDescriptor + offset);
	for (uint32 i = 0; i < numDescriptors; ++i)
	{
		dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, descriptorHandleIncrementSize);
	}

	staleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

void dx_dynamic_descriptor_heap::commitStagedDescriptors(dx_command_list* commandList, std::function<void(ID3D12GraphicsCommandList*, uint32, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
	uint32 numDescriptorsToCommit = computeStaleDescriptorCount();

	if (numDescriptorsToCommit > 0)
	{
		ID3D12GraphicsCommandList2* d3d12CommandList = commandList->getD3D12CommandList().Get();
		assert(d3d12CommandList != nullptr);

		if (!currentDescriptorHeap || numFreeHandles < numDescriptorsToCommit)
		{
			currentDescriptorHeap = requestDescriptorHeap();
			currentCPUDescriptorHandle = currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			currentGPUDescriptorHandle = currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			numFreeHandles = numDescriptorsPerHeap;

			commandList->setDescriptorHeap(descriptorHeapType, currentDescriptorHeap);

			// When updating the descriptor heap on the command list, all descriptor
			// tables must be (re)recopied to the new descriptor heap (not just
			// the stale descriptor tables).
			staleDescriptorTableBitMask = descriptorTableBitMask;
		}

		DWORD rootIndex;
		while (_BitScanForward(&rootIndex, staleDescriptorTableBitMask))
		{
			UINT numSrcDescriptors = descriptorTableCache[rootIndex].numDescriptors;
			D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorHandles = descriptorTableCache[rootIndex].baseDescriptor;

			D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStarts[] =
			{
				currentCPUDescriptorHandle
			};
			UINT destDescriptorRangeSizes[] =
			{
				numSrcDescriptors
			};

			device->CopyDescriptors(1, destDescriptorRangeStarts, destDescriptorRangeSizes,
				numSrcDescriptors, srcDescriptorHandles, nullptr, descriptorHeapType);

			setFunc(d3d12CommandList, rootIndex, currentGPUDescriptorHandle);

			currentCPUDescriptorHandle.Offset(numSrcDescriptors, descriptorHandleIncrementSize);
			currentGPUDescriptorHandle.Offset(numSrcDescriptors, descriptorHandleIncrementSize);
			numFreeHandles -= numSrcDescriptors;

			staleDescriptorTableBitMask ^= (1 << rootIndex);
		}
	}
}

void dx_dynamic_descriptor_heap::commitStagedDescriptorsForDraw(dx_command_list* commandList)
{
	commitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void dx_dynamic_descriptor_heap::commitStagedDescriptorsForDispatch(dx_command_list* commandList)
{
	commitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

void dx_dynamic_descriptor_heap::setCurrentDescriptorHeap(dx_command_list* commandList)
{
	commandList->setDescriptorHeap(descriptorHeapType, currentDescriptorHeap);
}

D3D12_GPU_DESCRIPTOR_HANDLE dx_dynamic_descriptor_heap::copyDescriptor(dx_command_list* comandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
	if (!currentDescriptorHeap || numFreeHandles < 1)
	{
		currentDescriptorHeap = requestDescriptorHeap();
		currentCPUDescriptorHandle = currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		currentGPUDescriptorHandle = currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		numFreeHandles = numDescriptorsPerHeap;

		comandList->setDescriptorHeap(descriptorHeapType, currentDescriptorHeap.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		staleDescriptorTableBitMask = descriptorTableBitMask;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE hGPU = currentGPUDescriptorHandle;
	device->CopyDescriptorsSimple(1, currentCPUDescriptorHandle, cpuDescriptor, descriptorHeapType);

	currentCPUDescriptorHandle.Offset(1, descriptorHandleIncrementSize);
	currentGPUDescriptorHandle.Offset(1, descriptorHandleIncrementSize);
	numFreeHandles -= 1;

	return hGPU;
}

void dx_dynamic_descriptor_heap::parseRootSignature(const dx_root_signature& rootSignature)
{
	staleDescriptorTableBitMask = 0;
	const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc = rootSignature.desc;

	descriptorTableBitMask = rootSignature.getDescriptorTableBitMask(descriptorHeapType);

	uint32 bitmask = descriptorTableBitMask;
	uint32 currentOffset = 0;
	DWORD rootIndex;
	while (_BitScanForward(&rootIndex, bitmask) && rootIndex < rootSignatureDesc.NumParameters)
	{
		uint32 numDescriptors = rootSignature.numDescriptorsPerTable[rootIndex];

		descriptor_table_cache& cache = descriptorTableCache[rootIndex];
		cache.numDescriptors = numDescriptors;
		cache.baseDescriptor = &descriptorHandleCache[currentOffset];

		currentOffset += numDescriptors;

		// Flip the descriptor table bit so it's not scanned again for the current index.
		bitmask ^= (1 << rootIndex);
	}

	assert(currentOffset <= numDescriptorsPerHeap && 
		"The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}

void dx_dynamic_descriptor_heap::reset()
{
	freeDescriptorHeaps = descriptorHeapPool;
	currentDescriptorHeap.Reset();
	currentCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	currentGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	numFreeHandles = 0;
	descriptorTableBitMask = 0;
	staleDescriptorTableBitMask = 0;

	// Reset the table cache
	for (int i = 0; i < maxDescriptorTables; ++i)
	{
		descriptorTableCache[i].reset();
	}
}

ComPtr<ID3D12DescriptorHeap> dx_dynamic_descriptor_heap::requestDescriptorHeap()
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	if (!freeDescriptorHeaps.empty())
	{
		descriptorHeap = freeDescriptorHeaps.front();
		freeDescriptorHeaps.pop();
	}
	else
	{
		descriptorHeap = createDescriptorHeap();
		descriptorHeapPool.push(descriptorHeap);
	}

	return descriptorHeap;
}

ComPtr<ID3D12DescriptorHeap> dx_dynamic_descriptor_heap::createDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = descriptorHeapType;
	descriptorHeapDesc.NumDescriptors = numDescriptorsPerHeap;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	checkResult(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

uint32 dx_dynamic_descriptor_heap::computeStaleDescriptorCount() const
{
	uint32_t numStaleDescriptors = 0;
	DWORD i;
	DWORD staleDescriptorsBitMask = staleDescriptorTableBitMask;

	while (_BitScanForward(&i, staleDescriptorsBitMask))
	{
		numStaleDescriptors += descriptorTableCache[i].numDescriptors;
		staleDescriptorsBitMask ^= (1 << i);
	}

	return numStaleDescriptors;
}
