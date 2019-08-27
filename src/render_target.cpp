#include "pch.h"
#include "render_target.h"


void dx_render_target::attachColorTexture(uint32 attachmentPoint, dx_texture& texture)
{
	assert(texture.resource != nullptr);

	colorAttachments[attachmentPoint] = &texture;

	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	if (width == 0 || height == 0)
	{
		width = (uint32)desc.Width;
		height = desc.Height;
		viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
	}
	else
	{
		assert(width == desc.Width && height == desc.Height);
	}
	
	renderTargetFormat.NumRenderTargets = 0;
	for (uint32 i = 0; i < arraysize(colorAttachments); ++i)
	{
		const dx_texture* tex = colorAttachments[i];
		if (tex && tex->resource) 
		{
			renderTargetFormat.RTFormats[renderTargetFormat.NumRenderTargets++] = tex->resource->GetDesc().Format;
		}
	}
}

void dx_render_target::attachDepthStencilTexture(dx_texture& texture)
{
	assert(texture.resource != nullptr);

	depthStencilAttachment = &texture;

	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	if (width == 0 || height == 0)
	{
		width = (uint32)desc.Width;
		height = desc.Height;
		viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
	}
	else
	{
		assert(width == desc.Width && height == desc.Height);
	}

	depthStencilFormat = dx_texture::getDepthFormatFromTypeless(desc.Format);
}

void dx_render_target::resize(uint32 width, uint32 height)
{
	width = max(width, 1u);
	height = max(height, 1u);

	this->width = width;
	this->height = height;

	for (uint32 i = 0; i < arraysize(colorAttachments); ++i)
	{
		dx_texture* tex = colorAttachments[i];
		if (tex && tex->resource)
		{
			tex->resize(width, height);
		}
	}

	if (depthStencilAttachment && depthStencilAttachment->resource)
	{
		depthStencilAttachment->resize(width, height);
	}

	viewport.Width = (float)width;
	viewport.Height = (float)height;
}
