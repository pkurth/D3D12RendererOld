#pragma once

#include "common.h"
#include "math.h"
#include "root_signature.h"

struct alignas(16) generate_mips_cb
{
	uint32 srcMipLevel;				// Texture level of source mip
	uint32 numMipLevelsToGenerate;	// The shader can generate up to 4 mips at once.
	uint32 srcDimensionFlags;		// Flags specifying whether width and height are even or odd (see above).
	uint32 isSRGB;					// Must apply gamma correction to sRGB textures.
	vec2 texelSize;					// 1.0 / OutMip1.Dimensions
};

enum generate_mips_root_parameter
{
	generate_mips_param_constant_buffer,
	generate_mips_param_src,
	generate_mips_param_out,

	generate_mips_num_params
};

struct dx_generate_mips_pso
{
	void initialize(ComPtr<ID3D12Device2> device);

	dx_root_signature rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;
	D3D12_CPU_DESCRIPTOR_HANDLE defaultUAV;
};
