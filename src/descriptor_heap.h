#pragma once

#include "common.h"
#include "texture.h"
#include "buffer.h"

struct dx_descriptor_heap
{
	void initialize(ComPtr<ID3D12Device2> device, uint32 numDescriptors);

	CD3DX12_GPU_DESCRIPTOR_HANDLE push2DTexture(dx_texture& texture);
	CD3DX12_GPU_DESCRIPTOR_HANDLE pushCubemap(dx_texture& texture);
	CD3DX12_GPU_DESCRIPTOR_HANDLE pushDepthTexture(dx_texture& texture);
	CD3DX12_GPU_DESCRIPTOR_HANDLE pushNullTexture();
	CD3DX12_GPU_DESCRIPTOR_HANDLE pushStructuredBuffer(dx_structured_buffer& buffer);


	ComPtr<ID3D12Device2> device;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	uint32 descriptorHandleIncrementSize;


	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};
