#pragma once

#include "common.h"
#include "descriptor_allocator.h"

#include <dx/d3dx12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <string>
#include <mutex>
#include <unordered_map>

struct dx_resource
{
	void initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource);
	void initialize(ComPtr<ID3D12Device2> device, const D3D12_RESOURCE_DESC& resourceDesc);

	ComPtr<ID3D12Resource> resource;

	bool checkFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
	bool checkFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;

	virtual D3D12_CPU_DESCRIPTOR_HANDLE getShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const = 0;
	virtual D3D12_CPU_DESCRIPTOR_HANDLE getUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const = 0;

	ComPtr<ID3D12Device2> device;
};

struct dx_vertex_buffer : dx_resource
{
	D3D12_VERTEX_BUFFER_VIEW view;

	D3D12_CPU_DESCRIPTOR_HANDLE getShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;
	D3D12_CPU_DESCRIPTOR_HANDLE getUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;
};

struct dx_index_buffer : dx_resource
{
	D3D12_INDEX_BUFFER_VIEW view;

	D3D12_CPU_DESCRIPTOR_HANDLE getShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;
	D3D12_CPU_DESCRIPTOR_HANDLE getUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;
};

enum texture_usage
{
	texture_usage_albedo,
	texture_usage_normal,
	texture_usage_metallic,
	texture_usage_roughness,
	texture_usage_height,
	texture_usage_render_target, 
};

struct dx_texture : dx_resource
{
	void initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource, texture_usage usage, const std::wstring name);
	void initialize(ComPtr<ID3D12Device2> device, const D3D12_RESOURCE_DESC& resourceDesc, texture_usage usage, const std::wstring name);

	void createViews();

	static bool isUAVCompatibleFormat(DXGI_FORMAT format);
	static bool isSRGBFormat(DXGI_FORMAT format);
	static bool isBGRFormat(DXGI_FORMAT format);
	static bool isDepthFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getTypelessFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getSRGBFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getUAVCompatableFormat(DXGI_FORMAT format);

	bool checkSRVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
	}

	bool checkRTVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
	}

	bool checkDSVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
	}

	bool checkUAVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			checkFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			checkFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
	}

	inline texture_usage getUsage() const { return usage; }
	inline const std::wstring& getName() const { return name; }

	D3D12_CPU_DESCRIPTOR_HANDLE getShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;
	D3D12_CPU_DESCRIPTOR_HANDLE getUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;
	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() const;

private:
	std::wstring name;
	texture_usage usage;

	mutable std::unordered_map<size_t, dx_descriptor_allocation> shaderResourceViews;
	mutable std::unordered_map<size_t, dx_descriptor_allocation> unorderedAccessViews;

	mutable std::mutex shaderResourceViewsMutex;
	mutable std::mutex unorderedAccessViewsMutex;

	dx_descriptor_allocation renderTargetView;
	dx_descriptor_allocation depthStencilView;
};
