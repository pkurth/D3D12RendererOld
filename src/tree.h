#pragma once

#include "buffer.h"
#include "root_signature.h"
#include "render_target.h"
#include "command_list.h"
#include "indirect_drawing.h"


struct tree_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget, DXGI_FORMAT shadowMapFormat);
	
	void render(dx_command_list* commandList,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress,
		const indirect_descriptor_heap& descriptors);

	void renderDepthOnly(dx_command_list* commandList, const render_camera& camera);

	dx_mesh mesh;
	std::vector<submesh_info> submeshes;

	animation_skeleton skeleton;

	ComPtr<ID3D12PipelineState> lightingPipelineState;
	dx_root_signature lightingRootSignature;

	ComPtr<ID3D12PipelineState> depthOnlyPipelineState;
	dx_root_signature depthOnlyRootSignature;
};
