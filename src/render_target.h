#pragma once

#include "texture.h"

struct dx_render_target
{
	void attachColorTexture(uint32 attachmentPoint, dx_texture& texture);
	void attachDepthStencilTexture(dx_texture& texture);
	void resize(uint32 width, uint32 height);

	dx_texture* colorAttachments[8] = {};
	dx_texture* depthStencilAttachment = nullptr;
	D3D12_VIEWPORT viewport;

	D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
	DXGI_FORMAT depthStencilFormat = {};

	uint32 width = 0;
	uint32 height = 0;
};
