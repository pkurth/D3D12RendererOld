#pragma once

#include "common.h"
#include "resource.h"
#include "resource_state_tracker.h"

#include <d3d12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

#include <vector>
#include <string>

class dx_command_list
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType);
	
	void transitionResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES afterState, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);
	void transitionResource(const dx_resource& resource, D3D12_RESOURCE_STATES afterState, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);

	void clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);
	void clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

	void updateBufferResource(ComPtr<ID3D12Resource>& destinationResource,
		size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	template <typename vertex_t>
	dx_vertex_buffer createVertexBuffer(vertex_t* vertices, uint32 count);

	template <typename index_t>
	dx_index_buffer createIndexBuffer(index_t* indices, uint32 count);

	void copyTextureSubresource(dx_texture& texture, uint32 firstSubresource, uint32 numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData);
	dx_texture loadTextureFromFile(const std::wstring& filename, texture_usage usage);

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

	bool close(dx_command_list* pendingCommandList);
	void close();

	inline ComPtr<ID3D12GraphicsCommandList2> getD3D12CommandList() const { return commandList; }

private:

	void trackObject(ComPtr<ID3D12Object> object);


	D3D12_COMMAND_LIST_TYPE				commandListType;
	ComPtr<ID3D12Device2>				device;
	ComPtr<ID3D12CommandAllocator>		commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2>	commandList;

	std::vector<ComPtr<ID3D12Object>>	trackedObjects;

	dx_resource_state_tracker			resourceStateTracker;
};

template <typename vertex_t>
dx_vertex_buffer dx_command_list::createVertexBuffer(vertex_t* vertices, uint32 count)
{
	dx_vertex_buffer result;

	updateBufferResource(
		result.resource,
		count, sizeof(vertex_t), vertices);
	
	result.view.BufferLocation = result.resource->GetGPUVirtualAddress();
	result.view.SizeInBytes = count * sizeof(vertex_t);
	result.view.StrideInBytes = sizeof(vertex_t);

	return result;
}

template <typename index_t> inline DXGI_FORMAT getFormat() { static_assert(false, "Unknown pixel format"); return DXGI_FORMAT_UNKNOWN; }
template <>					inline DXGI_FORMAT getFormat<uint16>() { return DXGI_FORMAT_R16_UINT; }
template <>					inline DXGI_FORMAT getFormat<uint32>() { return DXGI_FORMAT_R32_UINT; }

template <typename index_t>
dx_index_buffer dx_command_list::createIndexBuffer(index_t* indices, uint32 count)
{
	dx_index_buffer result;

	updateBufferResource(
		result.resource,
		count, sizeof(index_t), indices);

	result.view.BufferLocation = result.resource->GetGPUVirtualAddress();
	result.view.Format = getFormat<index_t>();
	result.view.SizeInBytes = count * sizeof(index_t);

	return result;
}
