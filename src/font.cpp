#include "pch.h"
#include "font.h"
#include "common.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

bool dx_font::initialize(dx_command_list* commandList, const char* fontname, int pixelHeight)
{
	char filename[512];
	sprintf(filename, "C:/Windows/Fonts/%s.ttf", fontname);

	FILE* file = fopen(filename, "rb");
	if (!file)
	{
		const char* fallback = "arial";
		if (strcmp(fontname, fallback) != 0)
		{
			std::cout << "Failed to load font " << fontname << ". falling back to " << fallback << "." << std::endl;
			return initialize(commandList, fallback, pixelHeight);
		}
		else
		{
			std::cout << "Failed to load fallback font " << fallback << "." << std::endl;
			return false;
		}
	}

	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8* ttfBuffer = new uint8[fsize + 1];
	fread(ttfBuffer, fsize, 1, file);
	fclose(file);

	stbtt_fontinfo font;
	stbtt_InitFont(&font, ttfBuffer, stbtt_GetFontOffsetForIndex(ttfBuffer, 0));


	float scale = stbtt_ScaleForPixelHeight(&font, (float)pixelHeight);
	int ascent;
	stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
	int baseline = (int)(ascent * scale);


	stbtt_packedchar packedChars[NUM_CODEPOINTS];
	stbtt_pack_context ctx;

	this->height = (float)pixelHeight;

	int currentPackSize = 256;

	int packResult;
	do
	{
		uint8* bitmap = new uint8[currentPackSize * currentPackSize];

		stbtt_PackBegin(&ctx, bitmap, currentPackSize, currentPackSize, currentPackSize, 1, nullptr);
		stbtt_pack_range range = { 0 };
		range.font_size = (float)pixelHeight;
		range.first_unicode_codepoint_in_range = FIRST_CODEPOINT;
		range.num_chars = NUM_CODEPOINTS;
		range.chardata_for_range = packedChars;
		packResult = stbtt_PackFontRanges(&ctx, ttfBuffer, 0, &range, 1);
		stbtt_PackEnd(&ctx);

		if (packResult)
		{
			commandList->loadTextureFromMemory(atlas, bitmap, currentPackSize, currentPackSize, DXGI_FORMAT_R8_UNORM, texture_type_noncolor, true);
		}

		delete[] bitmap;

		currentPackSize *= 2;
	} while (!packResult);

	currentPackSize /= 2;

	glyphs.resize(NUM_CODEPOINTS);
	advanceX.resize(NUM_CODEPOINTS * NUM_CODEPOINTS);
	for (int i = 0; i < NUM_CODEPOINTS; ++i)
	{
		font_glyph& g = glyphs[i];
		stbtt_packedchar& p = packedChars[i];
		g.left = (float)p.x0 / (currentPackSize - 1);
		g.top = (float)p.y0 / (currentPackSize - 1);
		g.right = (float)p.x1 / (currentPackSize - 1);
		g.bottom = (float)p.y1 / (currentPackSize - 1);
		g.width = p.x1 - p.x0;
		g.height = p.y1 - p.y0;
		g.offsetX = (int)p.xoff;
		g.offsetY = (int)p.yoff;

		int advanceX = (int)p.xadvance;
		for (int j = 0; j < NUM_CODEPOINTS; ++j)
		{
			int kerning = (int)(stbtt_GetCodepointKernAdvance(&font, i + FIRST_CODEPOINT, j + FIRST_CODEPOINT) * scale);
			this->advanceX[i * NUM_CODEPOINTS + j] = advanceX + kerning;
		}
	}

	delete[] ttfBuffer;
	return true;
}
