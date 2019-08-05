#pragma once

#include "texture.h"
#include "command_list.h"
#include <vector>

#define FIRST_CODEPOINT ' '
#define LAST_CODEPOINT '~'
#define NUM_CODEPOINTS (LAST_CODEPOINT - FIRST_CODEPOINT + 1)

struct font_glyph
{
	float left;
	float top;
	float right;
	float bottom;
	int width;
	int height;
	int offsetX;
	int offsetY;
};

struct dx_font
{
	float height;
	dx_texture atlas;
	std::vector<font_glyph> glyphs;
	std::vector<int> advanceX;

	bool initialize(dx_command_list* commandList, const char* fontname, int pixelHeight);
};


