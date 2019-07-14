#pragma once

#include "resource.h"

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
	void initialize(ComPtr<ID3D12Device2> device, texture_usage usage, const D3D12_RESOURCE_DESC& resourceDesc);
	void initialize(ComPtr<ID3D12Device2> device, texture_usage usage, ComPtr<ID3D12Resource> resource);
	void initialize(const dx_texture& other);

	dx_texture() {}
	dx_texture(const dx_texture& other);

	static bool isUAVCompatibleFormat(DXGI_FORMAT format);
	static bool isSRGBFormat(DXGI_FORMAT format);
	static bool isBGRFormat(DXGI_FORMAT format);
	static bool isDepthFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getTypelessFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getSRGBFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getUAVCompatableFormat(DXGI_FORMAT format);

	bool checkRTVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
	}

	bool checkDSVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
	}

	texture_usage usage;

	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetView() { return renderTargetView; }
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() { return depthStencilView; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView;
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView;
};
