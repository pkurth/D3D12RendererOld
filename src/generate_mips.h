#pragma once

#include "common.h"
#include "math.h"

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
	generate_mips_constant_buffer,
	generate_mips_src,
	generate_mips_out,

	generate_mips_num_params
};

