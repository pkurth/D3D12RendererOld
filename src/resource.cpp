#include "pch.h"
#include "resource.h"
#include "error.h"

namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
	template <typename T>
	inline void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	template<>
	struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC>
	{
		std::size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, srvDesc.Format);
			hash_combine(seed, srvDesc.ViewDimension);
			hash_combine(seed, srvDesc.Shader4ComponentMapping);

			switch (srvDesc.ViewDimension)
			{
			case D3D12_SRV_DIMENSION_BUFFER:
				hash_combine(seed, srvDesc.Buffer.FirstElement);
				hash_combine(seed, srvDesc.Buffer.NumElements);
				hash_combine(seed, srvDesc.Buffer.StructureByteStride);
				hash_combine(seed, srvDesc.Buffer.Flags);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1D:
				hash_combine(seed, srvDesc.Texture1D.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture1D.MipLevels);
				hash_combine(seed, srvDesc.Texture1D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, srvDesc.Texture1DArray.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture1DArray.MipLevels);
				hash_combine(seed, srvDesc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, srvDesc.Texture1DArray.ArraySize);
				hash_combine(seed, srvDesc.Texture1DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:
				hash_combine(seed, srvDesc.Texture2D.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture2D.MipLevels);
				hash_combine(seed, srvDesc.Texture2D.PlaneSlice);
				hash_combine(seed, srvDesc.Texture2D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, srvDesc.Texture2DArray.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture2DArray.MipLevels);
				hash_combine(seed, srvDesc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, srvDesc.Texture2DArray.ArraySize);
				hash_combine(seed, srvDesc.Texture2DArray.PlaneSlice);
				hash_combine(seed, srvDesc.Texture2DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
				//                hash_combine(seed, srvDesc.Texture2DMS.UnusedField_NothingToDefine);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
				hash_combine(seed, srvDesc.Texture2DMSArray.FirstArraySlice);
				hash_combine(seed, srvDesc.Texture2DMSArray.ArraySize);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:
				hash_combine(seed, srvDesc.Texture3D.MostDetailedMip);
				hash_combine(seed, srvDesc.Texture3D.MipLevels);
				hash_combine(seed, srvDesc.Texture3D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				hash_combine(seed, srvDesc.TextureCube.MostDetailedMip);
				hash_combine(seed, srvDesc.TextureCube.MipLevels);
				hash_combine(seed, srvDesc.TextureCube.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				hash_combine(seed, srvDesc.TextureCubeArray.MostDetailedMip);
				hash_combine(seed, srvDesc.TextureCubeArray.MipLevels);
				hash_combine(seed, srvDesc.TextureCubeArray.First2DArrayFace);
				hash_combine(seed, srvDesc.TextureCubeArray.NumCubes);
				hash_combine(seed, srvDesc.TextureCubeArray.ResourceMinLODClamp);
				break;
				// TODO: Update Visual Studio?
				//case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
				//    hash_combine(seed, srvDesc.RaytracingAccelerationStructure.Location);
				//    break;
			}

			return seed;
		}
	};

	template<>
	struct hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>
	{
		std::size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, uavDesc.Format);
			hash_combine(seed, uavDesc.ViewDimension);

			switch (uavDesc.ViewDimension)
			{
			case D3D12_UAV_DIMENSION_BUFFER:
				hash_combine(seed, uavDesc.Buffer.FirstElement);
				hash_combine(seed, uavDesc.Buffer.NumElements);
				hash_combine(seed, uavDesc.Buffer.StructureByteStride);
				hash_combine(seed, uavDesc.Buffer.CounterOffsetInBytes);
				hash_combine(seed, uavDesc.Buffer.Flags);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1D:
				hash_combine(seed, uavDesc.Texture1D.MipSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, uavDesc.Texture1DArray.MipSlice);
				hash_combine(seed, uavDesc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, uavDesc.Texture1DArray.ArraySize);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:
				hash_combine(seed, uavDesc.Texture2D.MipSlice);
				hash_combine(seed, uavDesc.Texture2D.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, uavDesc.Texture2DArray.MipSlice);
				hash_combine(seed, uavDesc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, uavDesc.Texture2DArray.ArraySize);
				hash_combine(seed, uavDesc.Texture2DArray.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE3D:
				hash_combine(seed, uavDesc.Texture3D.MipSlice);
				hash_combine(seed, uavDesc.Texture3D.FirstWSlice);
				hash_combine(seed, uavDesc.Texture3D.WSize);
				break;
			}

			return seed;
		}
	};
}

dx_resource::dx_resource(const dx_resource& other)
{
	this->device = other.device;
	this->formatSupport = other.formatSupport;
	this->resource = other.resource;
	this->shaderResourceViews = other.shaderResourceViews;
	this->unorderedAccessViews = other.unorderedAccessViews;
}

void dx_resource::initialize(ComPtr<ID3D12Device2> device, const D3D12_RESOURCE_DESC& resourceDesc, D3D12_CLEAR_VALUE* clearValue)
{
	this->device = device;

	checkResult(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		clearValue,
		IID_PPV_ARGS(&resource)));

	formatSupport.Format = resourceDesc.Format;
	checkResult(device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	shaderResourceViews.clear();
	unorderedAccessViews.clear();
}

void dx_resource::initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource)
{
	this->device = device;
	this->resource = resource;

	D3D12_RESOURCE_DESC resourceDesc(resource->GetDesc());

	formatSupport.Format = resourceDesc.Format;
	checkResult(device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	shaderResourceViews.clear();
	unorderedAccessViews.clear();
}

D3D12_CPU_DESCRIPTOR_HANDLE dx_resource::getShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
	size_t hash = 0;
	if (srvDesc)
	{
		hash = std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(*srvDesc);
	}

	std::lock_guard<std::mutex> lock(srvMutex);

	auto iter = shaderResourceViews.find(hash);
	if (iter == shaderResourceViews.end())
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srv = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).getDescriptorHandle(0);
		device->CreateShaderResourceView(resource.Get(), srvDesc, srv);
		iter = shaderResourceViews.insert({ hash, srv }).first;
	}

	return iter->second;
}

D3D12_CPU_DESCRIPTOR_HANDLE dx_resource::getUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc)
{
	size_t hash = 0;
	if (uavDesc)
	{
		hash = std::hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>{}(*uavDesc);
	}

	std::lock_guard<std::mutex> lock(uavMutex);

	auto iter = unorderedAccessViews.find(hash);
	if (iter == unorderedAccessViews.end())
	{
		D3D12_CPU_DESCRIPTOR_HANDLE uav = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).getDescriptorHandle(0);
		device->CreateUnorderedAccessView(resource.Get(), nullptr, uavDesc, uav);
		iter = unorderedAccessViews.insert({ hash, uav }).first;
	}

	return iter->second;
}

bool dx_resource::checkFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const
{
	return (this->formatSupport.Support1 & formatSupport) != 0;
}

bool dx_resource::checkFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const
{
	return (this->formatSupport.Support2 & formatSupport) != 0;
}
