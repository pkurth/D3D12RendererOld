#pragma once

#include "common.h"


class dx_command_list;
struct dx_root_signature;

class dx_dynamic_descriptor_heap
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32 numDescriptorsPerHeap = 1024);

	void stageDescriptors(uint32 rootParameterIndex, uint32 offset, uint32 numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor);

	void commitStagedDescriptorsForDraw(dx_command_list* commandList);
	void commitStagedDescriptorsForDispatch(dx_command_list* commandList);

	D3D12_GPU_DESCRIPTOR_HANDLE copyDescriptor(dx_command_list* comandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

	void parseRootSignature(const dx_root_signature& rootSignature);

	void reset();

private:
	ComPtr<ID3D12Device2> device;
	ComPtr<ID3D12DescriptorHeap> requestDescriptorHeap();
	ComPtr<ID3D12DescriptorHeap> createDescriptorHeap();

	uint32 computeStaleDescriptorCount() const;

	void commitStagedDescriptors(dx_command_list* commandList, std::function<void(ID3D12GraphicsCommandList*, uint32, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

	static const uint32 maxDescriptorTables = 32;


	struct descriptor_table_cache
	{
		descriptor_table_cache()
			: numDescriptors(0)
			, baseDescriptor(nullptr)
		{}

		// Reset the table cache.
		void reset()
		{
			numDescriptors = 0;
			baseDescriptor = nullptr;
		}

		uint32_t numDescriptors;
		D3D12_CPU_DESCRIPTOR_HANDLE* baseDescriptor;
	};

	// Valid values are:
	// D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	// D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
	D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType;

	uint32 numDescriptorsPerHeap;
	uint32 descriptorHandleIncrementSize;

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> descriptorHandleCache;
	descriptor_table_cache descriptorTableCache[maxDescriptorTables];

	// Each bit in the bit mask represents the index in the root signature that contains a descriptor table.
	uint32 descriptorTableBitMask;
	uint32 staleDescriptorTableBitMask;

	std::queue<ComPtr<ID3D12DescriptorHeap>> descriptorHeapPool;
	std::queue<ComPtr<ID3D12DescriptorHeap>> freeDescriptorHeaps;


	ComPtr<ID3D12DescriptorHeap> currentDescriptorHeap;
	CD3DX12_GPU_DESCRIPTOR_HANDLE currentGPUDescriptorHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE currentCPUDescriptorHandle;

	uint32 numFreeHandles;
};
