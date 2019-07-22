#include "pch.h"
#include "render_target.h"


void dx_render_target::attachTexture(render_target_attachment_point attachmentPoint, const dx_texture& texture)
{
	assert(texture.resource != nullptr);

	attachments[attachmentPoint] = texture;

	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	if (width == 0 || height == 0)
	{
		width = (uint32)desc.Width;
		height = desc.Height;
	}
	else
	{
		assert(width == desc.Width && height == desc.Height);
	}

	viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f, };

	
	renderTargetFormat.NumRenderTargets = 0;
	for (uint32 i = 0; i <= render_target_attachment_point_color7; ++i)
	{
		const dx_texture& tex = attachments[i];
		if (tex.resource != nullptr) 
		{
			renderTargetFormat.RTFormats[renderTargetFormat.NumRenderTargets++] = tex.resource->GetDesc().Format;
		}
	}

	if (attachmentPoint == render_target_attachment_point_depthstencil)
	{
		depthStencilFormat = desc.Format;
	}
}
