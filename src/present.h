#pragma once

#include "root_signature.h"
#include "command_list.h"

#define PRESENT_ROOTPARAM_CAMERA		0
#define PRESENT_ROOTPARAM_MODE			1
#define PRESENT_ROOTPARAM_TONEMAP		2
#define PRESENT_ROOTPARAM_TEXTURE		3

struct present_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, const D3D12_RT_FORMAT_ARRAY& renderTargetFormat);
	void render(dx_command_list* commandList, dx_texture& hdrTexture);

	ComPtr<ID3D12PipelineState> pipelineState;
	dx_root_signature rootSignature;
};
