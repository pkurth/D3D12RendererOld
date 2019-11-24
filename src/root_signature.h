#pragma once

#include "common.h"


struct dx_root_signature
{
	void initialize(ComPtr<ID3D12Device2> device, const D3D12_ROOT_SIGNATURE_DESC1& desc, bool parse = true);
	void shutdown();

	dx_root_signature() { desc = { }; }
	~dx_root_signature() { shutdown(); }

	uint32 getDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const;
	
	D3D12_ROOT_SIGNATURE_DESC1 desc;
	ComPtr<ID3D12RootSignature> rootSignature;

	uint32 descriptorTableBitMask;
	uint32 samplerTableBitMask;

	uint32 numDescriptorsPerTable[32];
};

