#pragma once

#include "texture.h"

struct dx_render_target
{
	void attachColorTexture(uint32 attachmentPoint, const dx_texture& texture);
	void attachDepthStencilTexture(const dx_texture& texture);

	dx_texture colorAttachments[8];
	dx_texture depthStencilAttachment;
	D3D12_VIEWPORT viewport;

	D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
	DXGI_FORMAT depthStencilFormat = {};

	uint32 width = 0;
	uint32 height = 0;
};
