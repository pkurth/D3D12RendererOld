#include "pch.h"
#include "texture.h"
#include "error.h"
#include "descriptor_allocator.h"
#include "common.h"


dx_texture::dx_texture(const dx_texture& other)
	: dx_resource(other)
{
	this->depthStencilView = other.depthStencilView;
	this->renderTargetView = other.renderTargetView;
	this->usage = other.usage;
	this->depthStencilView = other.depthStencilView;
	this->renderTargetView = other.renderTargetView;
}

dx_texture& dx_texture::operator=(const dx_texture& other)
{
	this->resource = other.resource;
	this->device = other.device;
	this->depthStencilView = other.depthStencilView;
	this->renderTargetView = other.renderTargetView;
	this->usage = other.usage;
	this->depthStencilView = other.depthStencilView;
	this->renderTargetView = other.renderTargetView;

	return *this;
}

void dx_texture::initialize(ComPtr<ID3D12Device2> device, texture_usage usage, const D3D12_RESOURCE_DESC& resourceDesc, D3D12_CLEAR_VALUE* clearValue)
{
	this->usage = usage;

	dx_resource::initialize(device, resourceDesc, clearValue);
	
	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		checkRTVSupport())
	{
		renderTargetView = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).getDescriptorHandle(0);
		device->CreateRenderTargetView(resource.Get(), nullptr, renderTargetView);
	}
	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
		checkDSVSupport())
	{
		depthStencilView = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).getDescriptorHandle(0);
		device->CreateDepthStencilView(resource.Get(), nullptr, depthStencilView);
	}
}

void dx_texture::initialize(ComPtr<ID3D12Device2> device, texture_usage usage, ComPtr<ID3D12Resource> resource)
{
	this->usage = usage;

	dx_resource::initialize(device, resource);

	D3D12_RESOURCE_DESC resourceDesc(resource->GetDesc());

	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		checkRTVSupport())
	{
		renderTargetView = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).getDescriptorHandle(0);
		device->CreateRenderTargetView(resource.Get(), nullptr, renderTargetView);
	}
	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
		checkDSVSupport())
	{
		depthStencilView = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).getDescriptorHandle(0);
		device->CreateDepthStencilView(resource.Get(), nullptr, depthStencilView);
	}
}

void dx_texture::initialize(const dx_texture& other)
{
	this->usage = other.usage;
	this->device = other.device;
	this->resource = other.resource;
	this->shaderResourceViews = other.shaderResourceViews;
	this->unorderedAccessViews = other.unorderedAccessViews;
	this->depthStencilView = other.depthStencilView;
	this->renderTargetView = other.renderTargetView;
}

bool dx_texture::isUAVCompatibleFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SINT:
		return true;
	default:
		return false;
	}
}

bool dx_texture::isSRGBFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

bool dx_texture::isBGRFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

bool dx_texture::isDepthFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_D16_UNORM:
		return true;
	default:
		return false;
	}
}

DXGI_FORMAT dx_texture::getTypelessFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT typelessFormat = format;

	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32B32A32_TYPELESS;
		break;
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32B32_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
		typelessFormat = DXGI_FORMAT_R16G16B16A16_TYPELESS;
		break;
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		typelessFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
		typelessFormat = DXGI_FORMAT_R10G10B10A2_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
		typelessFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
		typelessFormat = DXGI_FORMAT_R16G16_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
		typelessFormat = DXGI_FORMAT_R32_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		typelessFormat = DXGI_FORMAT_R8G8_TYPELESS;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
		typelessFormat = DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
		typelessFormat = DXGI_FORMAT_R8_TYPELESS;
		break;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC1_TYPELESS;
		break;
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC2_TYPELESS;
		break;
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC3_TYPELESS;
		break;
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		typelessFormat = DXGI_FORMAT_BC4_TYPELESS;
		break;
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		typelessFormat = DXGI_FORMAT_BC5_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_B8G8R8X8_TYPELESS;
		break;
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
		typelessFormat = DXGI_FORMAT_BC6H_TYPELESS;
		break;
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC7_TYPELESS;
		break;
	}

	return typelessFormat;
}

DXGI_FORMAT dx_texture::getSRGBFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT srgbFormat = format;
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		srgbFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC1_UNORM:
		srgbFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC2_UNORM:
		srgbFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC3_UNORM:
		srgbFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		srgbFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		srgbFormat = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC7_UNORM:
		srgbFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
		break;
	}

	return srgbFormat;
}

DXGI_FORMAT dx_texture::getUAVCompatibleFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT uavFormat = format;

	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		uavFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	}

	return uavFormat;
}