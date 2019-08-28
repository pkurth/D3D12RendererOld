#pragma once

#include "root_signature.h"
#include "render_target.h"
#include "command_list.h"

#define LIGHTING_ROOTPARAM_CAMERA		0
#define LIGHTING_ROOTPARAM_TEXTURES		1
#define LIGHTING_ROOTPARAM_DIRECTIONAL	2


struct lighting_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget);
	void render(dx_command_list* commandList, 
		dx_texture irradiance, dx_texture prefilteredEnvironment,
		dx_texture& albedoAOTexture, dx_texture& normalRoughnessMetalnessTexture, dx_texture& depthTexture,
		dx_texture* sunShadowMapCascades, uint32 numSunShadowCascades,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress);

	ComPtr<ID3D12PipelineState> pipelineState;
	dx_root_signature rootSignature;

	D3D12_CPU_DESCRIPTOR_HANDLE defaultSRV;

	dx_texture brdf;
};
