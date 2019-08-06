#pragma once

#include "texture.h"
#include "command_list.h"
#include <vector>

#define FIRST_CODEPOINT '!'
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
	std::vector<float> advanceX;

	float spaceWidth;

	bool initialize(dx_command_list* commandList, const char* fontname, int pixelHeight, bool sdf);

	float getAdvance(uint32 fromCP, uint32 toCP)
	{
		if (!toCP || fromCP == ' ')
		{
			return spaceWidth;
		}
		else if (toCP == ' ')
		{
			return advanceX[(fromCP - FIRST_CODEPOINT) * (uint64)(NUM_CODEPOINTS + 1) + NUM_CODEPOINTS];
		}
		else
		{
			return advanceX[(fromCP - FIRST_CODEPOINT) * (uint64)(NUM_CODEPOINTS + 1) + (toCP - FIRST_CODEPOINT)];
		}
	}
};


