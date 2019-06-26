#include "command_list.h"
#include "error.h"

#include <dx/d3dx12.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <filesystem>

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
}

void dx_command_list::transitionResource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES afterState, uint32 subresource, bool flushBarriers)
{
	if (resource)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COMMON, afterState, subresource);
		resourceStateTracker.resourceBarrier(barrier);
	}

	if (flushBarriers)
	{
		resourceStateTracker.flushResourceBarriers(this);
	}
}

void dx_command_list::transitionResource(const dx_resource& resource, D3D12_RESOURCE_STATES afterState, uint32 subresource, bool flushBarriers)
{
	transitionResource(resource.resource, afterState, subresource, flushBarriers);
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
		resourceStateTracker.flushResourceBarriers(this);

		UpdateSubresources(commandList.Get(),
			destinationResource.Get(), intermediateResource.Get(),
			0, 0, 1, &subresourceData);

		trackObject(intermediateResource);
	}

	trackObject(destinationResource);
}

void dx_command_list::copyTextureSubresource(dx_texture& texture, uint32 firstSubresource, uint32 numSubresources, D3D12_SUBRESOURCE_DATA* subresourceData)
{
	//ComPtr<ID3D12Resource> destinationResource = texture.textureResource;
	//if (destinationResource)
	//{
	//	// Resource must be in the copy-destination state.
	//	TransitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST);
	//	FlushResourceBarriers();

	//	UINT64 requiredSize = GetRequiredIntermediateSize(destinationResource.Get(), firstSubresource, numSubresources);

	//	// Create a temporary (intermediate) resource for uploading the subresources
	//	ComPtr<ID3D12Resource> intermediateResource;
	//	ThrowIfFailed(device->CreateCommittedResource(
	//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//		D3D12_HEAP_FLAG_NONE,
	//		&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
	//		D3D12_RESOURCE_STATE_GENERIC_READ,
	//		nullptr,
	//		IID_PPV_ARGS(&intermediateResource)
	//	));

	//	UpdateSubresources(m_d3d12CommandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);

	//	TrackObject(intermediateResource);
	//	TrackObject(destinationResource);
	//}
}

dx_texture dx_command_list::loadTextureFromFile(const std::wstring& filename, texture_usage usage)
{
	dx_texture result;
	//result.usage = usage;
	//result.name = filename;
	//result.createViews();

	//std::filesystem::path path(filename);
	//assert(std::filesystem::exists(path));

	//auto it = textureCache.find(filename);
	//if (it != textureCache.end())
	//{
	//	result.resource = it->second;
	//}
	//else
	//{
	//	DirectX::TexMetadata metadata;
	//	DirectX::ScratchImage scratchImage;

	//	ComPtr<ID3D12Resource> textureResource;

	//	if (path.extension() == ".dds")
	//	{
	//		checkResult(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
	//	}
	//	else if (path.extension() == ".hdr")
	//	{
	//		checkResult(DirectX::LoadFromHDRFile(filename.c_str(), &metadata, scratchImage));
	//	}
	//	else if (path.extension() == ".tga")
	//	{
	//		checkResult(DirectX::LoadFromTGAFile(filename.c_str(), &metadata, scratchImage));
	//	}
	//	else
	//	{
	//		checkResult(DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratchImage));
	//	}

	//	if (usage == texture_usage_albedo)
	//	{
	//		metadata.format = DirectX::MakeSRGB(metadata.format);
	//	}

	//	D3D12_RESOURCE_DESC textureDesc = {};
	//	switch (metadata.dimension)
	//	{
	//	case DirectX::TEX_DIMENSION_TEXTURE1D:
	//		textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, metadata.width, metadata.arraySize);
	//		break;
	//	case DirectX::TEX_DIMENSION_TEXTURE2D:
	//		textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, metadata.height, metadata.arraySize);
	//		break;
	//	case DirectX::TEX_DIMENSION_TEXTURE3D:
	//		textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, metadata.width, metadata.height, metadata.depth);
	//		break;
	//	default:
	//		assert(false);
	//		break;
	//	}

	//	checkResult(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
	//		D3D12_HEAP_FLAG_NONE,
	//		&textureDesc,
	//		D3D12_RESOURCE_STATE_COMMON,
	//		nullptr,
	//		IID_PPV_ARGS(&textureResource)));

	//	uint32 numImages = scratchImage.GetImageCount();

	//	dx_resource_state_tracker::addGlobalResourceState(textureResource.Get(), D3D12_RESOURCE_STATE_COMMON, numImages);

	//	result.resource = textureResource;

	//	const DirectX::Image* pImages = scratchImage.GetImages();
	//	std::vector<D3D12_SUBRESOURCE_DATA> subresources(numImages);
	//	for (uint32 i = 0; i < numImages; ++i)
	//	{
	//		D3D12_SUBRESOURCE_DATA& subresource = subresources[i];
	//		subresource.RowPitch = pImages[i].rowPitch;
	//		subresource.SlicePitch = pImages[i].slicePitch;
	//		subresource.pData = pImages[i].pixels;
	//	}

	//	copyTextureSubresource(result, 0, numImages, subresources.data());

	//	if (numImages < textureResource->GetDesc().MipLevels)
	//	{
	//		generateMips(result);
	//	}

	//	// Add the texture resource to the texture cache.
	//	std::lock_guard<std::mutex> lock(textureCacheMutex);
	//	textureCache[filename] = textureResource.Get();
	//}

	return result;
}

void dx_command_list::setPipelineState(ComPtr<ID3D12PipelineState> pipelineState)
{
	commandList->SetPipelineState(pipelineState.Get());
}

void dx_command_list::setRootSignature(ComPtr<ID3D12RootSignature> rootSignature)
{
	commandList->SetGraphicsRootSignature(rootSignature.Get());
}

void dx_command_list::setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
{
	commandList->IASetPrimitiveTopology(topology);
}

void dx_command_list::setVertexBuffer(uint32 slot, dx_vertex_buffer& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer.view);
}

void dx_command_list::setIndexBuffer(dx_index_buffer& buffer)
{
	commandList->IASetIndexBuffer(&buffer.view);
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
	commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void dx_command_list::drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance)
{
	commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void dx_command_list::reset()
{
	checkResult(commandAllocator->Reset());
	checkResult(commandList->Reset(commandAllocator.Get(), nullptr));

	resourceStateTracker.reset();
	trackedObjects.clear();
}

bool dx_command_list::close(dx_command_list* pendingCommandList)
{
	resourceStateTracker.flushResourceBarriers(this);

	checkResult(commandList->Close());

	uint32 numPendingBarriers = resourceStateTracker.flushPendingResourceBarriers(pendingCommandList);
	resourceStateTracker.commitFinalResourceStates();

	return numPendingBarriers > 0;
}

void dx_command_list::close()
{
	resourceStateTracker.flushResourceBarriers(this);

	checkResult(commandList->Close());
}

void dx_command_list::trackObject(ComPtr<ID3D12Object> object)
{
	trackedObjects.push_back(object);
}
