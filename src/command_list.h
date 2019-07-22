#pragma once

#include "common.h"
#include "buffer.h"
#include "texture.h"
#include "resource_state_tracker.h"
#include "dynamic_descriptor_heap.h"
#include "generate_mips.h"
#include "brdf.h"
#include "model.h"
#include "render_target.h"



class dx_command_list
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType);
	
	void transitionBarrier(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES afterState, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);
	void transitionBarrier(const dx_resource& resource, D3D12_RESOURCE_STATES afterState, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);

	void uavBarrier(ComPtr<ID3D12Resource> resource, bool flushBarriers = false);
	void uavBarrier(const dx_resource& resource, bool flushBarriers = false);

	void aliasingBarrier(ComPtr<ID3D12Resource> beforeResource, ComPtr<ID3D12Resource> afterResource, bool flushBarriers = false);
	void aliasingBarrier(const dx_resource& beforeResource, const dx_resource& afterResource, bool flushBarriers = false);

	void copyResource(ComPtr<ID3D12Resource> dstRes, ComPtr<ID3D12Resource> srcRes);
	void copyResource(dx_resource& dstRes, const dx_resource& srcRes);


	

	// Buffer creation.
	template <typename vertex_t> dx_vertex_buffer createVertexBuffer(vertex_t* vertices, uint32 count); 
	template <typename index_t> dx_index_buffer createIndexBuffer(index_t* indices, uint32 count);
	template <typename vertex_t> dx_mesh createMesh(const cpu_mesh<vertex_t>& model);

	// Texture creation.
	void loadTextureFromFile(dx_texture& texture, const std::wstring& filename, texture_usage usage, bool genMips = true);
	void copyTextureForReadback(dx_texture& texture, ComPtr<ID3D12Resource>& readbackBuffer, uint32 numMips = 0);
	void convertEquirectangularToCubemap(dx_texture& equirectangular, dx_texture& cubemap, uint32 resolution, uint32 numMips, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
	void createIrradianceMap(dx_texture& environment, dx_texture& irradiance, uint32 resolution = 32);
	void prefilterEnvironmentMap(dx_texture& environment, dx_texture& prefiltered, uint32 resolution = 128);
	void integrateBRDF(dx_texture& brdf, uint32 resolution = 512);

	// Pipeline.
	void setPipelineState(ComPtr<ID3D12PipelineState> pipelineState);

	// Root signature.
	void setGraphicsRootSignature(const dx_root_signature& rootSignature);
	void setComputeRootSignature(const dx_root_signature& rootSignature);
	void setGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T> void setGraphics32BitConstants(uint32 rootParameterIndex, const T& constants);
	void setCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T> void setCompute32BitConstants(uint32 rootParameterIndex, const T& constants);
	void setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap);

	void setShaderResourceView(uint32 rootParameterIndex,
		uint32 descriptorOffset,
		dx_resource& resource,
		D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		uint32 firstSubresource = 0,
		uint32 numSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srv = nullptr);

	void setUnorderedAccessView(uint32 rootParameterIndex,
		uint32 descriptorOffset,
		dx_resource& resource,
		D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		uint32 firstSubresource = 0,
		uint32 numSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav = nullptr);

	// Input assembly.
	void setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void setVertexBuffer(uint32 slot, dx_vertex_buffer& buffer);
	void setIndexBuffer(dx_index_buffer& buffer);

	// Rasterizer.
	void setViewport(const D3D12_VIEWPORT& viewport);
	void setScissor(const D3D12_RECT& scissor);

	// Render targets.
	void setScreenRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, uint32 numRTVs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv); // Does not transition. Assumes that screen is always in write-state.
	void setRenderTarget(dx_render_target& renderTarget); // Also transitions the targets to write-state.
	void clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);
	void clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.f);

	// Draw.
	void draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);

	// Dispatch.
	void dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);


	// End frame.
	void reset();
	bool close(dx_command_list* pendingCommandList);
	void close();

	inline ComPtr<ID3D12GraphicsCommandList2> getD3D12CommandList() const { return commandList; }
	inline dx_command_list* getComputeCommandList() const { return computeCommandList; }

private:

	void updateBufferResource(ComPtr<ID3D12Resource>& destinationResource,
		size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	void trackObject(ComPtr<ID3D12Object> object);
	void flushResourceBarriers();

	void copyTextureSubresource(dx_texture& texture, uint32 firstSubresource, uint32 numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData);
	void generateMips(dx_texture& texture);


	D3D12_COMMAND_LIST_TYPE				commandListType;
	ComPtr<ID3D12Device2>				device;
	ComPtr<ID3D12CommandAllocator>		commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2>	commandList;

	std::vector<ComPtr<ID3D12Object>>	trackedObjects;

	dx_resource_state_tracker			resourceStateTracker;
	dx_dynamic_descriptor_heap			dynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	ID3D12DescriptorHeap*				descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	dx_command_list*					computeCommandList;

	dx_generate_mips_pso				generateMipsPSO;
	dx_equirectangular_to_cubemap_pso	equirectangularToCubemapPSO;
	dx_cubemap_to_irradiance_pso		cubemapToIrradiancePSO;
	dx_prefilter_environment_pso		prefilterEnvironmentPSO;
	dx_integrate_brdf_pso				integrateBrdfPSO;
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
	result.numIndices = count;

	return result;
}

template<typename vertex_t>
inline dx_mesh dx_command_list::createMesh(const cpu_mesh<vertex_t>& model)
{
	dx_mesh result;
	result.vertexBuffer = createVertexBuffer(model.vertices.data(), (uint32)model.vertices.size());
	result.indexBuffer = createIndexBuffer((uint32*)model.triangles.data(), (uint32)model.triangles.size() * 3);
	return result;
}

template<typename T>
inline void dx_command_list::setGraphics32BitConstants(uint32 rootParameterIndex, const T& constants)
{
	static_assert(sizeof(T) % sizeof(uint32) == 0, "Size of type must be a multiple of 4 bytes");
	setGraphics32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32), &constants);
}

template<typename T>
inline void dx_command_list::setCompute32BitConstants(uint32 rootParameterIndex, const T& constants)
{
	static_assert(sizeof(T) % sizeof(uint32) == 0, "Size of type must be a multiple of 4 bytes");
	setCompute32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32), &constants);
}
