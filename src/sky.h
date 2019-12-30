#pragma once

#include "buffer.h"
#include "root_signature.h"
#include "render_target.h"
#include "command_list.h"

#define SKY_ROOTPARAM_CAMERA			0
#define SKY_ROOTPARAM_TEXTURE			1

struct sky_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget);
	void render(dx_command_list* commandList, D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, dx_texture& cubemap);

	dx_mesh mesh;

	ComPtr<ID3D12PipelineState> pipelineState;
	dx_root_signature rootSignature;
};
