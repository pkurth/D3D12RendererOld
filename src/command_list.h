#pragma once

#include "common.h"
#include "buffer.h"
#include "texture.h"
#include "resource_state_tracker.h"
#include "dynamic_descriptor_heap.h"
#include "upload_buffer.h"
#include "generate_mips.h"
#include "brdf.h"
#include "model.h"
#include "render_target.h"

class dx_command_list
{
public:
	void initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType);
	
	// Barriers.
	void transitionBarrier(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES afterState, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);
	void transitionBarrier(const dx_resource& resource, D3D12_RESOURCE_STATES afterState, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool flushBarriers = false);

	void uavBarrier(ComPtr<ID3D12Resource> resource, bool flushBarriers = false);
	void uavBarrier(const dx_resource& resource, bool flushBarriers = false);

	void aliasingBarrier(ComPtr<ID3D12Resource> beforeResource, ComPtr<ID3D12Resource> afterResource, bool flushBarriers = false);
	void aliasingBarrier(const dx_resource& beforeResource, const dx_resource& afterResource, bool flushBarriers = false);


	// Buffer copy.
	void copyResource(ComPtr<ID3D12Resource> dstRes, ComPtr<ID3D12Resource> srcRes, bool transitionDst = true);
	void copyResource(dx_resource& dstRes, const dx_resource& srcRes, bool transitionDst = true);

	void uploadBufferData(ComPtr<ID3D12Resource> destinationResource, const void* bufferData, uint32 bufferSize);
	void updateBufferDataRange(ComPtr<ID3D12Resource> destinationResource, const void* data, uint32 offset, uint32 size);


	// Texture creation.
	void loadTextureFromFile(dx_texture& texture, const std::wstring& filename, texture_type type, bool genMips = true);
	void loadTextureFromMemory(dx_texture& texture, const void* data, uint32 width, uint32 height, DXGI_FORMAT format, texture_type type, bool genMips = true);
	void copyTextureForReadback(dx_texture& texture, ComPtr<ID3D12Resource>& readbackBuffer, uint32 numMips = 0);

	void generateMips(dx_texture& texture);

	// BRDF.
	void convertEquirectangularToCubemap(dx_texture& equirectangular, dx_texture& cubemap, uint32 resolution, uint32 numMips, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
	void createIrradianceMap(dx_texture& environment, dx_texture& irradiance, uint32 resolution = 32, uint32 sourceSlice = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, float uvzScale = 1.f);
	void prefilterEnvironmentMap(dx_texture& environment, dx_texture& prefiltered, uint32 resolution = 128);
	void integrateBRDF(dx_texture& brdf, uint32 resolution = 512);
	void projectCubemapToSphericalHarmonics(dx_texture& cubemap, dx_structured_buffer& sh, uint32 srcMip, uint32 shIndex);


	// Pipeline.
	void setPipelineState(ComPtr<ID3D12PipelineState> pipelineState);


	// Root signature.
	void setGraphicsRootSignature(const dx_root_signature& rootSignature);
	void setComputeRootSignature(const dx_root_signature& rootSignature);
	void setGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T> void setGraphics32BitConstants(uint32 rootParameterIndex, const T& constants);
	void setCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T> void setCompute32BitConstants(uint32 rootParameterIndex, const T& constants);

	D3D12_GPU_VIRTUAL_ADDRESS uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template <typename T> D3D12_GPU_VIRTUAL_ADDRESS uploadDynamicConstantBuffer(const T& data);

	D3D12_GPU_VIRTUAL_ADDRESS uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data);
	template <typename T> D3D12_GPU_VIRTUAL_ADDRESS uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, const T& data);

	void setGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	D3D12_GPU_VIRTUAL_ADDRESS uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data);
	template <typename T> D3D12_GPU_VIRTUAL_ADDRESS uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, const T& data);

	void setComputeDynamicConstantBuffer(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	// These buffers are temporary! They only last one frame!
	template <typename vertex_t> D3D12_VERTEX_BUFFER_VIEW createDynamicVertexBuffer(const vertex_t* vertices, uint32 count);

	

	void setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ComPtr<ID3D12DescriptorHeap> heap);
	void resetToDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType);

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

	void bindCubemap(uint32 rootParameterIndex, uint32 descriptorOffset, dx_texture& cubemap,
		D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = cubemap.resource->GetDesc().Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = (uint32)-1; // Use all mips.

		setShaderResourceView(rootParameterIndex, descriptorOffset, cubemap, stateAfter, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &srvDesc);
	}

	void bindDepthTextureForReading(uint32 rootParameterIndex, uint32 descriptorOffset, dx_texture& depthTexture,
		D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = dx_texture::getReadFormatFromTypeless(depthTexture.resource->GetDesc().Format);
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		setShaderResourceView(rootParameterIndex, descriptorOffset, depthTexture, stateAfter, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &srvDesc);
	}

	void stageDescriptors(uint32 rootParameterIndex,
		uint32 descriptorOffset,
		uint32 count,
		D3D12_CPU_DESCRIPTOR_HANDLE baseDesc)
	{
		dynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].stageDescriptors(rootParameterIndex, descriptorOffset, count, baseDesc);
	}

	// Input assembly.
	void setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void setVertexBuffer(uint32 slot, dx_vertex_buffer& buffer);
	void setVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer);
	void setIndexBuffer(dx_index_buffer& buffer);

	// Rasterizer.
	void setViewport(const D3D12_VIEWPORT& viewport);
	void setScissor(const D3D12_RECT& scissor);

	// Render targets.
	void setScreenRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, uint32 numRTVs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv); // Does not transition. Assumes that screen is always in write-state.
	void setRenderTarget(dx_render_target& renderTarget, uint32 arraySlice = 0); // Also transitions the targets to write-state.
	void clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float* clearColor);
	void clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f);
	void clearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32 stencil = 0);
	void clearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.f, uint32 stencil = 0);
	void setStencilReference(uint32 stencilReference);

	// Draw.
	void draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);
	void drawIndirect(ComPtr<ID3D12CommandSignature> commandSignature, uint32 numDraws, dx_buffer commandBuffer);
	void drawIndirect(ComPtr<ID3D12CommandSignature> commandSignature, uint32 maxNumDraws, dx_buffer numDrawsBuffer, dx_buffer commandBuffer);

	// Dispatch.
	void dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);


	// End frame.
	void reset();
	bool close(ComPtr<ID3D12GraphicsCommandList2> pendingCommandList);
	void close();

	inline ComPtr<ID3D12GraphicsCommandList2> getD3D12CommandList() const { return commandList; }
	inline dx_command_list* getComputeCommandList() const { return computeCommandList; }

	void flushResourceBarriers();

//private:
	void trackObject(ComPtr<ID3D12Object> object);

	void copyTextureSubresource(dx_texture& texture, uint32 firstSubresource, uint32 numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData);


	D3D12_COMMAND_LIST_TYPE				commandListType;
	ComPtr<ID3D12Device2>				device;
	ComPtr<ID3D12CommandAllocator>		commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2>	commandList;

	std::vector<ComPtr<ID3D12Object>>	trackedObjects;

	dx_upload_buffer					uploadBuffer;
	dx_resource_state_tracker			resourceStateTracker;
	dx_dynamic_descriptor_heap			dynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	ID3D12DescriptorHeap*				descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	dx_command_list*					computeCommandList;

	dx_generate_mips_pso				generateMipsPSO;
	dx_equirectangular_to_cubemap_pso	equirectangularToCubemapPSO;
	dx_cubemap_to_irradiance_pso		cubemapToIrradiancePSO;
	dx_prefilter_environment_pso		prefilterEnvironmentPSO;
	dx_integrate_brdf_pso				integrateBrdfPSO;
	dx_cubemap_to_sh_pso				cubemapToSHPSO;
};

template<typename T>
inline void dx_command_list::setGraphics32BitConstants(uint32 rootParameterIndex, const T& constants)
{
	static_assert(sizeof(T) % sizeof(uint32) == 0, "Size of type must be a multiple of 4 bytes.");
	setGraphics32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32), &constants);
}

template<typename T>
inline void dx_command_list::setCompute32BitConstants(uint32 rootParameterIndex, const T& constants)
{
	static_assert(sizeof(T) % sizeof(uint32) == 0, "Size of type must be a multiple of 4 bytes.");
	setCompute32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32), &constants);
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS dx_command_list::uploadDynamicConstantBuffer(const T& data)
{
	return uploadDynamicConstantBuffer(sizeof(T), &data);
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS dx_command_list::uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, const T& data)
{
	return uploadAndSetGraphicsDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
}

template<typename T>
inline D3D12_GPU_VIRTUAL_ADDRESS dx_command_list::uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, const T& data)
{
	return uploadAndSetComputeDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
}

template<typename vertex_t>
inline D3D12_VERTEX_BUFFER_VIEW dx_command_list::createDynamicVertexBuffer(const vertex_t* vertices, uint32 count)
{
	uint32 sizeInBytes = count * sizeof(vertex_t);
	dx_upload_buffer::allocation allocation = uploadBuffer.allocate(sizeInBytes, sizeof(vertex_t));
	memcpy(allocation.cpu, vertices, sizeInBytes);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = allocation.gpu;
	vertexBufferView.SizeInBytes = sizeInBytes;
	vertexBufferView.StrideInBytes = sizeof(vertex_t);

	return vertexBufferView;
}
