#pragma once

#include "common.h"
#include "commands.h"

#include <dx/d3dx12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

struct dx_vertex_buffer
{
	ComPtr<ID3D12Resource> buffer;
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_index_buffer
{
	ComPtr<ID3D12Resource> buffer;
	D3D12_INDEX_BUFFER_VIEW view;
};

template <typename vertex_t>
static dx_vertex_buffer createVertexBuffer(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList2> commandList,
	vertex_t* vertices, uint32 count, ComPtr<ID3D12Resource>& intermediateVertexBuffer)
{
	dx_vertex_buffer result;

	updateBufferResource(device, commandList,
		&result.buffer, &intermediateVertexBuffer,
		count, sizeof(vertex_t), vertices);

	result.view.BufferLocation = result.buffer->GetGPUVirtualAddress();
	result.view.SizeInBytes = count * sizeof(vertex_t);
	result.view.StrideInBytes = sizeof(vertex_t);

	return result;
}

template <typename index_t>
static dx_index_buffer createIndexBuffer(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList2> commandList,
	index_t* indices, uint32 count, ComPtr<ID3D12Resource>& intermediateIndexBuffer)
{
	dx_index_buffer result;

	updateBufferResource(device, commandList,
		&result.buffer, &intermediateIndexBuffer,
		count, sizeof(index_t), indices);

	result.view.BufferLocation = result.buffer->GetGPUVirtualAddress();
	result.view.Format = DXGI_FORMAT_R16_UINT;
	result.view.SizeInBytes = count * sizeof(index_t);

	return result;
}
