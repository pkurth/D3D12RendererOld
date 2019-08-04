#pragma once

#include "texture.h"
#include "math.h"

// TODO: Probably change to pointers eventually.
struct dx_material
{
	dx_texture albedo;
	dx_texture normal;
	dx_texture roughMetal;
};
