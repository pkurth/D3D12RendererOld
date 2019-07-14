#pragma once

#include "common.h"
#include "descriptor_allocator.h"

#include <dx/d3dx12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <mutex>
#include <unordered_map>

struct dx_resource
{
	void initialize(ComPtr<ID3D12Device2> device, const D3D12_RESOURCE_DESC& resourceDesc);
	void initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource);

	dx_resource() {}
	dx_resource(const dx_resource& other);

	ComPtr<ID3D12Resource> resource;

	D3D12_CPU_DESCRIPTOR_HANDLE getShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc);
	D3D12_CPU_DESCRIPTOR_HANDLE getUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc);

	bool checkSRVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
	}

	bool checkUAVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			checkFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			checkFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
	}


protected:
	bool checkFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
	bool checkFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;

	std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> shaderResourceViews;
	std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> unorderedAccessViews;

	ComPtr<ID3D12Device2> device;

	std::mutex srvMutex;
	std::mutex uavMutex;
};
