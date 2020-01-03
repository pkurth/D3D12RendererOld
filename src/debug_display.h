#pragma once

#include "root_signature.h"
#include "command_list.h"
#include "camera.h"

#define DEBUG_DISPLAY_ROOTPARAM_CB		0
#define DEBUG_DISPLAY_ROOTPARAM_TEXTURE 1

struct debug_display
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget);

	void renderBillboard(dx_command_list* commandList, const render_camera& camera, vec3 position, vec2 dimensions, dx_texture& texture,
		bool keepAspectRatio = false, vec4 color = vec4(1.f, 1.f, 1.f, 1.f), bool isDepthTexture = false);
	void renderLineMesh(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh, mat4 transform);
	void renderLineStrip(dx_command_list* commandList, const render_camera& camera, vec3* vertices, uint32 numVertices, vec4 color);
	void renderLine(dx_command_list* commandList, const render_camera& camera, vec3 from, vec3 to, vec4 color);
	void renderFrustum(dx_command_list* commandList, const render_camera& camera, const camera_frustum_corners& frustum, vec4 color);

	ComPtr<ID3D12PipelineState> unlitTexturedPipelineState;
	dx_root_signature unlitTexturedRootSignature;

	ComPtr<ID3D12PipelineState> unlitLinePipelineState;
	ComPtr<ID3D12PipelineState> unlitFlatPipelineState;
	dx_root_signature unlitFlatRootSignature;

	dx_index_buffer frustumIndexBuffer;
};
