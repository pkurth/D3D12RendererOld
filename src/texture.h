#pragma once

#include "resource.h"

enum texture_type
{
	texture_type_color,
	texture_type_noncolor,
};

struct dx_texture : dx_resource
{
	void initialize(ComPtr<ID3D12Device2> device, D3D12_RESOURCE_DESC& resourceDesc, D3D12_CLEAR_VALUE* clearValue = nullptr);
	void initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource);
	void initialize(const dx_texture& other);

	dx_texture() {}
	dx_texture(const dx_texture& other);
	dx_texture& operator=(const dx_texture& other);

	void resize(uint32 width, uint32 height);

	static bool isUAVCompatibleFormat(DXGI_FORMAT format);
	static bool isSRGBFormat(DXGI_FORMAT format);
	static bool isBGRFormat(DXGI_FORMAT format);
	static bool isDepthFormat(DXGI_FORMAT format);
	static bool isTypelessFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getTypelessFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getSRGBFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getUAVCompatibleFormat(DXGI_FORMAT format);
	static DXGI_FORMAT getDepthFormatFromTypeless(DXGI_FORMAT format);
	static DXGI_FORMAT getReadFormatFromTypeless(DXGI_FORMAT format);
	static uint32 getFormatSize(DXGI_FORMAT format);

	bool checkRTVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
	}

	bool checkDSVSupport()
	{
		return checkFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetView() { return renderTargetView; }
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() { return depthStencilView; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView;
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView;
};
