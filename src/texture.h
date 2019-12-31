#pragma once

#include "resource.h"
#include "math.h"

enum texture_type
{
	texture_type_color,
	texture_type_noncolor,
};

struct dx_texture : dx_resource
{
	void initialize(ComPtr<ID3D12Device2> device, D3D12_RESOURCE_DESC resourceDesc, D3D12_CLEAR_VALUE* clearValue = nullptr);
	void initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource);
	void initialize(const dx_texture& other);

	DXGI_FORMAT format;

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

	D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetView(uint32 index = 0) { return renderTargetViews[index]; }
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() { return depthStencilView; }

private:
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetViews;
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView;
};

struct dx_texture_atlas : dx_texture
{
	uint32 slicesX;
	uint32 slicesY;

	void getUVs(uint32 x, uint32 y, vec2& uv0, vec2& uv1)
	{
		assert(x < slicesX);
		assert(y < slicesY);
		
		float width = 1.f / slicesX;
		float height = 1.f / slicesY;
		uv0 = vec2(x * width, y * height);
		uv1 = vec2((x + 1) * width, (y + 1) * height);
	}

	void getUVs(uint32 i, vec2& uv0, vec2& uv1)
	{
		uint32 x = i % slicesX;
		uint32 y = i / slicesX;
		getUVs(x, y, uv0, uv1);
	}
};
