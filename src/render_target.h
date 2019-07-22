#pragma once

#include "texture.h"

enum render_target_attachment_point
{
	render_target_attachment_point_color0,
	render_target_attachment_point_color1,
	render_target_attachment_point_color2,
	render_target_attachment_point_color3,
	render_target_attachment_point_color4,
	render_target_attachment_point_color5,
	render_target_attachment_point_color6,
	render_target_attachment_point_color7,
	render_target_attachment_point_depthstencil,

	render_target_num_attachment_points,
};

struct dx_render_target
{
	void attachTexture(render_target_attachment_point attachmentPoint, const dx_texture& texture);

	dx_texture attachments[render_target_num_attachment_points];
	D3D12_VIEWPORT viewport;

	D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
	DXGI_FORMAT depthStencilFormat = {};

	uint32 width = 0;
	uint32 height = 0;
};
