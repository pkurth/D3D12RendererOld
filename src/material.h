#pragma once

#include "texture.h"
#include "math.h"

// TODO: Probably change to pointers eventually.
struct dx_material
{
	dx_texture albedo;
	dx_texture normal;
	dx_texture roughness;
	dx_texture metallic;
};

#define USE_ALBEDO_TEXTURE		(1 << 0)
#define USE_NORMAL_TEXTURE		(1 << 1)
#define USE_ROUGHNESS_TEXTURE	(1 << 2)
#define USE_METALLIC_TEXTURE	(1 << 3)
#define USE_AO_TEXTURE			(1 << 4)

struct material_cb
{
	uint32 textureID;
	uint32 usageFlags;

	float roughnessOverride;
	float metallicOverride;

	vec4 albedoTint;
};


