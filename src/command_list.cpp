#include "command_list.h"
#include "error.h"
#include "command_queue.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <filesystem>
#include <iostream>

#include <DirectXTex/DirectXTex/DirectXTex.h>

static std::unordered_map<std::wstring, ID3D12Resource*> textureCache;
static std::mutex textureCacheMutex;


void dx_command_list::initialize(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE commandListType)
{
	this->device = device;
	this->commandListType = commandListType;
	checkResult(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));
	checkResult(device->CreateCommandList(0, commandListType, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	checkResult(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	resourceStateTracker.initialize();

	if (commandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		generateMipsPSO.initialize(device);
	}

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].initialize(device, (D3D12_DESCRIPTOR_HEAP_TYPE)i);
		descriptorHeaps[i] = nullptr;
	}
}

void dx_command_list::transitionBarrier(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES afterState, uint32 subresource, bool flushBarriers)
{
	if (resource)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COMMON, afterState, subresource);
		resourceStateTracker.resourceBarrier(barrier);
	}

	if (flushBarriers)
	{
		flushResourceBarriers();
	}
}

void dx_command_list::transitionBarrier(const dx_resource& resource, D3D12_RESOURCE_STATES afterState, uint32 subresource, bool flushBarriers)
{
	transitionBarrier(resource.resource, afterState, subresource, flushBarriers);
}

void dx_command_list::uavBarrier(ComPtr<ID3D12Resource> resource, bool flushBarriers)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	resourceStateTracker.resourceBarrier(barrier);

	if (flushBarriers)
	{
		flushResourceBarriers();
	}
}

void dx_command_list::uavBarrier(const dx_resource& resource, bool flushBarriers)
{
	uavBarrier(resource.resource, flushBarriers);
}

void dx_command_list::aliasingBarrier(ComPtr<ID3D12Resource> beforeResource, ComPtr<ID3D12Resource> afterResource, bool flushBarriers)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(beforeResource.Get(), afterResource.Get());
	resourceStateTracker.resourceBarrier(barrier);

	if (flushBarriers)
	{
		flushResourceBarriers();
	}
}

void dx_command_list::aliasingBarrier(const dx_resource& beforeResource, const dx_resource& afterResource, bool flushBarriers)
{
	aliasingBarrier(beforeResource.resource, afterResource.resource, flushBarriers);
}

void dx_command_list::copyResource(ComPtr<ID3D12Resource> dstRes, ComPtr<ID3D12Resource> srcRes)
{
	transitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
	transitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

	flushResourceBarriers();

	commandList->CopyResource(dstRes.Get(), srcRes.Get());

	trackObject(dstRes);
	trackObject(srcRes);
}

void dx_command_list::copyResource(dx_resource& dstRes, const dx_resource& srcRes)
{
	copyResource(dstRes.resource, srcRes.resource);
}

void dx_command_list::clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void dx_command_list::clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void dx_command_list::updateBufferResource(ComPtr<ID3D12Resource>& destinationResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numElements * elementSize;

	// Create a committed resource for the GPU resource in a default heap.
	checkResult(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&destinationResource)));

	dx_resource_state_tracker::addGlobalResourceState(destinationResource.Get(), D3D12_RESOURCE_STATE_COMMON, 1);

	if (bufferData)
	{
		ComPtr<ID3D12Resource> intermediateResource;

		// Create an committed resource for the upload.
		checkResult(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		resourceStateTracker.transitionResource(destinationResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
		flushResourceBarriers();

		UpdateSubresources(commandList.Get(),
			destinationResource.Get(), intermediateResource.Get(),
			0, 0, 1, &subresourceData);

		trackObject(intermediateResource);
	}

	trackObject(destinationResource);
}

void dx_command_list::copyTextureSubresource(dx_texture& texture, uint32 firstSubresource, uint32 numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData)
{
	ComPtr<ID3D12Resource> destinationResource = texture.resource;
	if (destinationResource)
	{
		transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST);
		flushResourceBarriers();

		UINT64 requiredSize = GetRequiredIntermediateSize(destinationResource.Get(), firstSubresource, numSubresources);

		// Create a temporary (intermediate) resource for uploading the subresources
		ComPtr<ID3D12Resource> intermediateResource;
		checkResult(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource)
		));

		UpdateSubresources(commandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);

		trackObject(intermediateResource);
		trackObject(destinationResource);
	}
}

void dx_command_list::loadTextureFromFile(dx_texture& texture, const std::wstring& filename, texture_usage usage)
{
	std::filesystem::path path(filename);
	assert(std::filesystem::exists(path));

	auto it = textureCache.find(filename);
	if (it != textureCache.end())
	{
		texture.initialize(device, it->second, usage, filename);
	}
	else
	{
		DirectX::TexMetadata metadata;
		DirectX::ScratchImage scratchImage;

		if (path.extension() == ".dds")
		{
			checkResult(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
		}
		else if (path.extension() == ".hdr")
		{
			checkResult(DirectX::LoadFromHDRFile(filename.c_str(), &metadata, scratchImage));
		}
		else if (path.extension() == ".tga")
		{
			checkResult(DirectX::LoadFromTGAFile(filename.c_str(), &metadata, scratchImage));
		}
		else
		{
			checkResult(DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratchImage));
		}

		if (usage == texture_usage_albedo)
		{
			metadata.format = DirectX::MakeSRGB(metadata.format);
		}

		D3D12_RESOURCE_DESC textureDesc = {};
		switch (metadata.dimension)
		{
		case DirectX::TEX_DIMENSION_TEXTURE1D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, metadata.width, (uint16)metadata.arraySize);
			break;
		case DirectX::TEX_DIMENSION_TEXTURE2D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, (uint32)metadata.height, (uint16)metadata.arraySize);
			break;
		case DirectX::TEX_DIMENSION_TEXTURE3D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, metadata.width, (uint32)metadata.height, (uint16)metadata.depth);
			break;
		default:
			assert(false);
			break;
		}

		texture.initialize(device, textureDesc, usage, filename);

		uint32 numMips = texture.resource->GetDesc().MipLevels;
		dx_resource_state_tracker::addGlobalResourceState(texture.resource.Get(), D3D12_RESOURCE_STATE_COMMON, numMips);

		const DirectX::Image* pImages = scratchImage.GetImages();
		uint32 numImages = (uint32)scratchImage.GetImageCount();
		std::vector<D3D12_SUBRESOURCE_DATA> subresources(numImages);
		for (uint32 i = 0; i < numImages; ++i)
		{
			D3D12_SUBRESOURCE_DATA& subresource = subresources[i];
			subresource.RowPitch = pImages[i].rowPitch;
			subresource.SlicePitch = pImages[i].slicePitch;
			subresource.pData = pImages[i].pixels;
		}

		copyTextureSubresource(texture, 0, numImages, subresources.data());

		if (numImages < numMips)
		{
			generateMips(texture);
		}

		// Add the texture resource to the texture cache.
		std::lock_guard<std::mutex> lock(textureCacheMutex);
		textureCache[filename] = texture.resource.Get();
	}
}

void dx_command_list::generateMips(dx_texture& texture)
{
	if (commandListType == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		if (!computeCommandList)
		{
			computeCommandList = dx_command_queue::computeCommandQueue.getAvailableCommandList();
		}
		computeCommandList->generateMips(texture);
		return;
	}

	ComPtr<ID3D12Resource> resource = texture.resource;
	if (!resource)
	{
		return;
	}

	D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

	uint32 numMips = resourceDesc.MipLevels;
	if (numMips == 1)
	{
		return;
	}

	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resourceDesc.DepthOrArraySize != 1 ||
		resourceDesc.SampleDesc.Count > 1)
	{
		std::cout << "GenerateMips is only supported for non-multi-sampled 2D Textures." << std::endl;
		return;
	}

	ComPtr<ID3D12Resource> uavResource = resource;
	ComPtr<ID3D12Resource> aliasResource; // In case the format of our texture does not support UAVs.

	if (!texture.checkUAVSupport() ||
		(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		D3D12_RESOURCE_DESC aliasDesc = resourceDesc;
		// Placed resources can't be render targets or depth-stencil views.
		aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		// Describe a UAV compatible resource that is used to perform
		// mipmapping of the original texture.
		D3D12_RESOURCE_DESC uavDesc = aliasDesc;   // The flags for the UAV description must match that of the alias description.
		uavDesc.Format = dx_texture::getUAVCompatableFormat(resourceDesc.Format);

		D3D12_RESOURCE_DESC resourceDescs[] = {
			aliasDesc,
			uavDesc
		};

		// Create a heap that is large enough to store a copy of the original resource.
		D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = device->GetResourceAllocationInfo(0, arraysize(resourceDescs), resourceDescs);

		D3D12_HEAP_DESC heapDesc = {};
		heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
		heapDesc.Alignment = allocationInfo.Alignment;
		heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

		ComPtr<ID3D12Heap> heap;
		checkResult(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));
		trackObject(heap);

		checkResult(device->CreatePlacedResource(
			heap.Get(),
			0,
			&aliasDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&aliasResource)
		));

		dx_resource_state_tracker::addGlobalResourceState(aliasResource.Get(), D3D12_RESOURCE_STATE_COMMON, numMips);
		trackObject(aliasResource);

		checkResult(device->CreatePlacedResource(
			heap.Get(),
			0,
			&uavDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&uavResource)
		));

		dx_resource_state_tracker::addGlobalResourceState(uavResource.Get(), D3D12_RESOURCE_STATE_COMMON, numMips);
		trackObject(uavResource);

		aliasingBarrier(nullptr, aliasResource);

		// Copy the original resource to the alias resource.
		copyResource(aliasResource, resource);

		// Add an aliasing barrier for the UAV compatible resource.
		aliasingBarrier(aliasResource, uavResource);
	}



	bool isSRGB = dx_texture::isSRGBFormat(resourceDesc.Format);
	setPipelineState(generateMipsPSO.pipelineState);
	setComputeRootSignature(generateMipsPSO.rootSignature);

	generate_mips_cb cb;
	cb.isSRGB = isSRGB;

	resourceDesc = uavResource->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = isSRGB ? dx_texture::getSRGBFormat(resourceDesc.Format) : resourceDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;  // Only 2D textures are supported.
	srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;

	dx_texture tmpTexture;
	tmpTexture.initialize(device, uavResource, texture.getUsage(), texture.getName());

	for (uint32 srcMip = 0; srcMip < resourceDesc.MipLevels - 1u; )
	{
		uint64 srcWidth = resourceDesc.Width >> srcMip;
		uint32 srcHeight = resourceDesc.Height >> srcMip;
		uint32 dstWidth = (uint32)(srcWidth >> 1);
		uint32 dstHeight = srcHeight >> 1;

		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		cb.srcDimensionFlags = (srcHeight & 1) << 1 | (srcWidth & 1);

		DWORD mipCount;

		// The number of times we can half the size of the texture and get
		// exactly a 50% reduction in size.
		// A 1 bit in the width or height indicates an odd dimension.
		// The case where either the width or the height is exactly 1 is handled
		// as a special case (as the dimension does not require reduction).
		_BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));

		// Maximum number of mips to generate is 4.
		mipCount = min<DWORD>(4, mipCount + 1);
		// Clamp to total number of mips left over.
		mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ?
			resourceDesc.MipLevels - srcMip - 1 : mipCount;

		// Dimensions should not reduce to 0.
		// This can happen if the width and height are not the same.
		dstWidth = std::max<DWORD>(1, dstWidth);
		dstHeight = std::max<DWORD>(1, dstHeight);

		cb.srcMipLevel = srcMip;
		cb.numMipLevelsToGenerate = mipCount;
		cb.texelSize.x = 1.f / (float)dstWidth;
		cb.texelSize.y = 1.f / (float)dstHeight;

		setCompute32BitConstants(generate_mips_param_constant_buffer, cb);
		setShaderResourceView(generate_mips_param_src, 0, tmpTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, srcMip, 1, &srvDesc);

		for (uint32 mip = 0; mip < mipCount; ++mip)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = resourceDesc.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = srcMip + mip + 1;

			setUnorderedAccessView(generate_mips_param_out, mip, tmpTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, srcMip + mip + 1, 1, &uavDesc);
		}

		if (mipCount < 4)
		{
			dynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].stageDescriptors(generate_mips_param_out, mipCount, 4 - mipCount, generateMipsPSO.getDefaultUAV());
		}

		dispatch(bucketize(dstWidth, 8), bucketize(dstHeight, 8));

		uavBarrier(uavResource);

		srcMip += mipCount;
	}

	if (aliasResource)
	{
		aliasingBarrier(uavResource, aliasResource);
		// Copy the alias resource back to the original resource.
		copyResource(resource, aliasResource);
	}
}

void dx_command_list::setPipelineState(ComPtr<ID3D12PipelineState> pipelineState)
{
	commandList->SetPipelineState(pipelineState.Get());
	trackObject(pipelineState);
}

void dx_command_list::setGraphicsRootSignature(const dx_root_signature& rootSignature)
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].parseRootSignature(rootSignature);
	}

	commandList->SetGraphicsRootSignature(rootSignature.rootSignature.Get());

	trackObject(rootSignature.rootSignature);
}

void dx_command_list::setComputeRootSignature(const dx_root_signature& rootSignature)
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].parseRootSignature(rootSignature);
	}

	commandList->SetComputeRootSignature(rootSignature.rootSignature.Get());

	trackObject(rootSignature.rootSignature);
}

void dx_command_list::setShaderResourceView(uint32 rootParameterIndex,
	uint32 descriptorOffset,
	const dx_resource& resource,
	D3D12_RESOURCE_STATES stateAfter,
	uint32 firstSubresource,
	uint32 numSubresources,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32 i = 0; i < numSubresources; ++i)
		{
			transitionBarrier(resource, stateAfter, firstSubresource + i);
		}
	}
	else
	{
		transitionBarrier(resource, stateAfter);
	}

	dynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].stageDescriptors(rootParameterIndex, descriptorOffset, 1, resource.getShaderResourceView(srv));

	trackObject(resource.resource);
}

void dx_command_list::setUnorderedAccessView(uint32 rootParameterIndex,
	uint32 descrptorOffset,
	const dx_resource& resource,
	D3D12_RESOURCE_STATES stateAfter,
	uint32 firstSubresource,
	uint32 numSubresources,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
{
	if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		for (uint32 i = 0; i < numSubresources; ++i)
		{
			transitionBarrier(resource, stateAfter, firstSubresource + i);
		}
	}
	else
	{
		transitionBarrier(resource, stateAfter);
	}

	dynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].stageDescriptors(rootParameterIndex, descrptorOffset, 1, resource.getUnorderedAccessView(uav));

	trackObject(resource.resource);
}

void dx_command_list::setGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants)
{
	commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void dx_command_list::setCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants)
{
	commandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void dx_command_list::setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
{
	commandList->IASetPrimitiveTopology(topology);
}

void dx_command_list::setVertexBuffer(uint32 slot, dx_vertex_buffer& buffer)
{
	transitionBarrier(buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	commandList->IASetVertexBuffers(slot, 1, &buffer.view);
	trackObject(buffer.resource);
}

void dx_command_list::setIndexBuffer(dx_index_buffer& buffer)
{
	transitionBarrier(buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	commandList->IASetIndexBuffer(&buffer.view);
	trackObject(buffer.resource);
}

void dx_command_list::setViewport(const D3D12_VIEWPORT& viewport)
{
	commandList->RSSetViewports(1, &viewport);
}

void dx_command_list::setScissor(const D3D12_RECT& scissor)
{
	commandList->RSSetScissorRects(1, &scissor);
}

void dx_command_list::draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance)
{
	flushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].commitStagedDescriptorsForDraw(this);
	}

	commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void dx_command_list::drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance)
{
	flushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].commitStagedDescriptorsForDraw(this);
	}

	commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void dx_command_list::dispatch(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ)
{
	flushResourceBarriers();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].commitStagedDescriptorsForDispatch(this);
	}

	commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void dx_command_list::reset()
{
	checkResult(commandAllocator->Reset());
	checkResult(commandList->Reset(commandAllocator.Get(), nullptr));

	resourceStateTracker.reset();
	trackedObjects.clear();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		dynamicDescriptorHeaps[i].reset();
		descriptorHeaps[i] = nullptr;
	}

	computeCommandList = nullptr;
}

bool dx_command_list::close(dx_command_list* pendingCommandList)
{
	flushResourceBarriers();

	checkResult(commandList->Close());

	uint32 numPendingBarriers = resourceStateTracker.flushPendingResourceBarriers(pendingCommandList);
	resourceStateTracker.commitFinalResourceStates();

	return numPendingBarriers > 0;
}

void dx_command_list::close()
{
	flushResourceBarriers();

	checkResult(commandList->Close());
}

void dx_command_list::trackObject(ComPtr<ID3D12Object> object)
{
	trackedObjects.push_back(object);
}

void dx_command_list::flushResourceBarriers()
{
	resourceStateTracker.flushResourceBarriers(this);
}

void dx_command_list::setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap)
{
	if (descriptorHeaps[heapType] != heap)
	{
		descriptorHeaps[heapType] = heap;
		

		uint32 numDescriptorHeaps = 0;
		ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

		for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			ID3D12DescriptorHeap* descriptorHeap = this->descriptorHeaps[i];
			if (descriptorHeap)
			{
				descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
			}
		}

		commandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);

	}
}

