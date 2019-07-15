#pragma once

#include "common.h"
#include "math.h"
#include "root_signature.h"

struct alignas(16) equirectangular_to_cubemap_cb
{
	uint32 cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint32 firstMip;				// The first mip level to generate.
	uint32 numMipLevelsToGenerate;	// The number of mips to generate.
};

enum equirectangular_to_cubemap_root_parameter
{
	equirectangular_to_cubemap_param_constant_buffer,
	equirectangular_to_cubemap_param_src,
	equirectangular_to_cubemap_param_out,

	equirectangular_to_cubemap_num_params
};

struct dx_equirectangular_to_cubemap_pso
{
	void initialize(ComPtr<ID3D12Device2> device);

	dx_root_signature rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	D3D12_CPU_DESCRIPTOR_HANDLE defaultUAV;
};



struct alignas(16) cubemap_to_irradiance_cb
{
	uint32 irradianceMapSize;
};

enum cubemap_to_irradiance_root_parameter
{
	cubemap_to_irradiance_param_constant_buffer,
	cubemap_to_irradiance_param_src,
	cubemap_to_irradiance_param_out,

	cubemap_to_irradiance_num_params
};

struct dx_cubemap_to_irradiance_pso
{
	void initialize(ComPtr<ID3D12Device2> device);

	dx_root_signature rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
};



struct alignas(16) prefilter_environment_cb
{
	uint32 cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint32 firstMip;				// The first mip level to generate.
	uint32 numMipLevelsToGenerate;	// The number of mips to generate.
	uint32 totalNumMipLevels;
};

enum prefilter_environment_root_parameter
{
	prefilter_environment_param_constant_buffer,
	prefilter_environment_param_src,
	prefilter_environment_param_out,

	prefilter_environment_num_params
};

struct dx_prefilter_environment_pso
{
	void initialize(ComPtr<ID3D12Device2> device);

	dx_root_signature rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	D3D12_CPU_DESCRIPTOR_HANDLE defaultUAV;
};
