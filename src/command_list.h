#pragma once

#include "common.h"
#include "buffer.h"

#include <d3d12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

class dx_command_list
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType);

	void transitionResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	void clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);
	void clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

	void updateBufferResource(ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
		size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	template <typename vertex_t>
	dx_vertex_buffer createVertexBuffer(vertex_t* vertices, uint32 count, ComPtr<ID3D12Resource>& intermediateVertexBuffer);

	template <typename index_t>
	dx_index_buffer createIndexBuffer(index_t* indices, uint32 count, ComPtr<ID3D12Resource>& intermediateIndexBuffer);

	void setPipelineState(ComPtr<ID3D12PipelineState> pipelineState);
	void setRootSignature(ComPtr<ID3D12RootSignature> rootSignature);

	// Input assembly.
	void setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void setVertexBuffer(uint32 slot, dx_vertex_buffer& buffer);
	void setIndexBuffer(dx_index_buffer& buffer);

	// Rasterizer.
	void setViewport(const D3D12_VIEWPORT& viewport);
	void setScissor(const D3D12_RECT& scissor);

	// Draw.
	void draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);

	void reset();
	void close();

	inline ComPtr<ID3D12GraphicsCommandList2> getD3D12CommandList() const { return commandList; }

private:
	D3D12_COMMAND_LIST_TYPE				commandListType;
	ComPtr<ID3D12Device2>				device;
	ComPtr<ID3D12CommandAllocator>		commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2>	commandList;
};

template <typename vertex_t>
dx_vertex_buffer dx_command_list::createVertexBuffer(vertex_t* vertices, uint32 count, ComPtr<ID3D12Resource>& intermediateVertexBuffer)
{
	dx_vertex_buffer result;

	updateBufferResource(
		&result.buffer, &intermediateVertexBuffer,
		count, sizeof(vertex_t), vertices);

	result.view.BufferLocation = result.buffer->GetGPUVirtualAddress();
	result.view.SizeInBytes = count * sizeof(vertex_t);
	result.view.StrideInBytes = sizeof(vertex_t);

	return result;
}

template <typename index_t>
dx_index_buffer dx_command_list::createIndexBuffer(index_t* indices, uint32 count, ComPtr<ID3D12Resource>& intermediateIndexBuffer)
{
	dx_index_buffer result;

	updateBufferResource(
		&result.buffer, &intermediateIndexBuffer,
		count, sizeof(index_t), indices);

	result.view.BufferLocation = result.buffer->GetGPUVirtualAddress();
	result.view.Format = DXGI_FORMAT_R16_UINT;
	result.view.SizeInBytes = count * sizeof(index_t);

	return result;
}
