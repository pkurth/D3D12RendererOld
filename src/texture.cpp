#include "pch.h"
#include "texture.h"
#include "error.h"
#include "descriptor_allocator.h"
#include "common.h"
#include "resource_state_tracker.h"


dx_texture::dx_texture(const dx_texture& other)
	: dx_resource(other)
{
	this->depthStencilView = other.depthStencilView;
	this->renderTargetViews = other.renderTargetViews;
	this->shaderResourceViews = other.shaderResourceViews;
	this->unorderedAccessViews = other.unorderedAccessViews;
}

dx_texture& dx_texture::operator=(const dx_texture& other)
{
	this->resource = other.resource;
	this->device = other.device;
	this->clearValueValid = other.clearValueValid;
	if (clearValueValid)
	{
		clearValue = other.clearValue;
	}
	this->depthStencilView = other.depthStencilView;
	this->renderTargetViews = other.renderTargetViews;
	this->shaderResourceViews = other.shaderResourceViews;
	this->unorderedAccessViews = other.unorderedAccessViews;

	return *this;
}

void dx_texture::resize(uint32 width, uint32 height)
{
	CD3DX12_RESOURCE_DESC resourceDesc(resource->GetDesc());

	if (width != resourceDesc.Width || height != resourceDesc.Height)
	{
		dx_resource_state_tracker::removeGlobalResourceState(resource.Get());

		resourceDesc.Width = max(width, 1u);
		resourceDesc.Height = max(height, 1u);

		D3D12_CLEAR_VALUE* cv = clearValueValid ? &clearValue : nullptr;

		checkResult(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			cv,
			IID_PPV_ARGS(&resource)
		));

		dx_resource_state_tracker::addGlobalResourceState(resource.Get(), D3D12_RESOURCE_STATE_COMMON, resourceDesc.MipLevels * resourceDesc.DepthOrArraySize);

		shaderResourceViews.clear();
		unorderedAccessViews.clear();

		// TODO: This leaks the old descriptors. Find a way to free them.
		if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
			checkRTVSupport())
		{
			if (resourceDesc.DepthOrArraySize > 1)
			{
				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
				rtvDesc.Format = resourceDesc.Format;
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.ArraySize = 1;
				rtvDesc.Texture2DArray.MipSlice = 0;
				rtvDesc.Texture2DArray.PlaneSlice = 0;

				dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, resourceDesc.DepthOrArraySize);
				for (uint32 i = 0; i < resourceDesc.DepthOrArraySize; ++i)
				{
					rtvDesc.Texture2DArray.FirstArraySlice = i;
					renderTargetViews[i] = allocation.getDescriptorHandle(i);
					device->CreateRenderTargetView(resource.Get(), &rtvDesc, renderTargetViews[i]);
				}
			}
			else
			{
				renderTargetViews[0] = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).getDescriptorHandle(0);
				device->CreateRenderTargetView(resource.Get(), nullptr, renderTargetViews[0]);
			}
		}
		if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
			(checkDSVSupport() || isDepthFormat(format)))
		{
			depthStencilView = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).getDescriptorHandle(0);

			if (isDepthFormat(format))
			{
				assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
				dsvDesc.Format = format;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				device->CreateDepthStencilView(resource.Get(), &dsvDesc, depthStencilView);
			}
			else
			{
				device->CreateDepthStencilView(resource.Get(), nullptr, depthStencilView);
			}
		}
	}
}

void dx_texture::initialize(ComPtr<ID3D12Device2> device, D3D12_RESOURCE_DESC resourceDesc, D3D12_CLEAR_VALUE* clearValue)
{
	format = resourceDesc.Format;
	if (isDepthFormat(format))
	{
		resourceDesc.Format = getTypelessFormat(format);
	}

	dx_resource::initialize(device, resourceDesc, clearValue);

	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		checkRTVSupport())
	{
		renderTargetViews.resize(resourceDesc.DepthOrArraySize);

		if (resourceDesc.DepthOrArraySize > 1)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.Format = resourceDesc.Format;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, resourceDesc.DepthOrArraySize);
			for (uint32 i = 0; i < resourceDesc.DepthOrArraySize; ++i)
			{
				rtvDesc.Texture2DArray.FirstArraySlice = i;
				renderTargetViews[i] = allocation.getDescriptorHandle(i);
				device->CreateRenderTargetView(resource.Get(), &rtvDesc, renderTargetViews[i]);
			}
		}
		else
		{
			renderTargetViews[0] = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).getDescriptorHandle(0);
			device->CreateRenderTargetView(resource.Get(), nullptr, renderTargetViews[0]);
		}
	}
	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
		(checkDSVSupport() || isDepthFormat(format)))
	{
		depthStencilView = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).getDescriptorHandle(0);

		if (isDepthFormat(format))
		{
			assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = format;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			device->CreateDepthStencilView(resource.Get(), &dsvDesc, depthStencilView);
		}
		else
		{
			device->CreateDepthStencilView(resource.Get(), nullptr, depthStencilView);
		}
	}
}

void dx_texture::initialize(ComPtr<ID3D12Device2> device, ComPtr<ID3D12Resource> resource)
{
	dx_resource::initialize(device, resource);

	D3D12_RESOURCE_DESC resourceDesc(resource->GetDesc());

	format = getDepthFormatFromTypeless(resourceDesc.Format);

	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		checkRTVSupport())
	{
		if (resourceDesc.DepthOrArraySize > 1)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.Format = resourceDesc.Format;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			dx_descriptor_allocation allocation = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, resourceDesc.DepthOrArraySize);
			for (uint32 i = 0; i < resourceDesc.DepthOrArraySize; ++i)
			{
				rtvDesc.Texture2DArray.FirstArraySlice = i;
				renderTargetViews[i] = allocation.getDescriptorHandle(i);
				device->CreateRenderTargetView(resource.Get(), &rtvDesc, renderTargetViews[i]);
			}
		}
		else
		{
			renderTargetViews[0] = dx_descriptor_allocator::allocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).getDescriptorHandle(0);
			device->CreateRenderTargetView(resource.Get(), nullptr, renderTargetViews[0]);
		}
	}
	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
		(checkDSVSupport() || isDepthFormat(format)))
	{
		if (isDepthFormat(format))
		{
			assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = format;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			device->CreateDepthStencilView(resource.Get(), &dsvDesc, depthStencilView);
		}
		else
		{
			device->CreateDepthStencilView(resource.Get(), nullptr, depthStencilView);
		}
	}
}

void dx_texture::initialize(const dx_texture& other)
{
	this->device = other.device;
	this->resource = other.resource;
	this->shaderResourceViews = other.shaderResourceViews;
	this->unorderedAccessViews = other.unorderedAccessViews;
	this->depthStencilView = other.depthStencilView;
	this->renderTargetViews = other.renderTargetViews;
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

bool dx_texture::isTypelessFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC7_TYPELESS:
		return true;
	}
	return false;
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
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		typelessFormat = DXGI_FORMAT_R24G8_TYPELESS;
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
		break;
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

DXGI_FORMAT dx_texture::getDepthFormatFromTypeless(DXGI_FORMAT format)
{
	// Incomplete list.
	switch (format)
	{
	case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_D16_UNORM;
	}
	
	return format;
}

DXGI_FORMAT dx_texture::getReadFormatFromTypeless(DXGI_FORMAT format)
{
	// Incomplete list.
	switch (format)
	{
	case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
	}

	return format;
}

uint32 dx_texture::getFormatSize(DXGI_FORMAT format)
{
	uint32 size = 0;

	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		size = 4 * 4;
		break;
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
	case DXGI_FORMAT_R32G32B32_TYPELESS:
		size = 3 * 4;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		size = 4 * 2;
		break;
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		size = 2 * 4;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		size = 4;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		size = 4;
		break;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
		size = 2 * 2;
		break;
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
		size = 4;
		break;
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R8G8_TYPELESS:
		size = 2;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
		size = 2;
		break;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_R8_TYPELESS:
		size = 1;
		break;
		size = 4;
		break;

	default:
		assert(false); // Compressed format.
	}

	return size;
}
