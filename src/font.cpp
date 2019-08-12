#include "pch.h"
#include "font.h"
#include "common.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

static void copyGlyphToAtlas(uint8* atlas, uint32 atlasWidth, uint32 atlasHeight, uint8* glyph, uint32 x, uint32 y, uint32 width, uint32 height)
{
	for (uint32 gy = 0; gy < height; ++gy)
	{
		for (uint32 gx = 0; gx < width; ++gx)
		{
			atlas[(gy + y) * atlasWidth + (gx + x)] = glyph[gy * width + gx];
		}
	}
}

bool dx_font::initialize(dx_command_list* commandList, const char* fontname, int pixelHeight, bool sdf)
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
			return initialize(commandList, fallback, pixelHeight, sdf);
		}
		else
		{
			std::cout << "Failed to load fallback font " << fallback << "." << std::endl;
			return false;
		}
	}

	this->height = (float)pixelHeight;

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

	glyphs.resize(NUM_CODEPOINTS);
	advanceX.resize(NUM_CODEPOINTS * (NUM_CODEPOINTS + 1));

	int spaceAdvanceX;
	stbtt_GetCodepointHMetrics(&font, ' ', &spaceAdvanceX, NULL);
	spaceWidth = spaceAdvanceX * scale;

	if (!sdf)
	{
		stbtt_packedchar packedChars[NUM_CODEPOINTS];
		stbtt_pack_context ctx;

		uint32 currentPackSize = 256;

		int32 packResult;
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

			for (int j = 0; j < NUM_CODEPOINTS; ++j)
			{
				float kerning = stbtt_GetCodepointKernAdvance(&font, i + FIRST_CODEPOINT, j + FIRST_CODEPOINT) * scale;
				this->advanceX[i * (NUM_CODEPOINTS + 1) + j] = p.xadvance + kerning;
			}
			float kerning = stbtt_GetCodepointKernAdvance(&font, i + FIRST_CODEPOINT, ' ') * scale;
			this->advanceX[i * (NUM_CODEPOINTS + 1) + NUM_CODEPOINTS] = p.xadvance + kerning;
		}
	}
	else
	{
		uint8* cpBitmaps[NUM_CODEPOINTS];
		stbrp_rect packRects[NUM_CODEPOINTS];

		uint32 extraPadding = 1;

		for (uint32 i = 0; i < NUM_CODEPOINTS; ++i)
		{
			int width, height;
			int offsetX, offsetY;
			int advanceX;
			cpBitmaps[i] = stbtt_GetCodepointSDF(&font, scale, i + FIRST_CODEPOINT, 5 + extraPadding, 128, 128.f / 5.f, &width, &height, &offsetX, &offsetY);
			stbtt_GetCodepointHMetrics(&font, i + FIRST_CODEPOINT, &advanceX, NULL);

			packRects[i].w = width;
			packRects[i].h = height;

			font_glyph& g = glyphs[i];
			g.width = width;
			g.height = height;
			g.offsetX = offsetX;
			g.offsetY = offsetY;

			float advance = advanceX * scale;

			for (int j = 0; j < NUM_CODEPOINTS; ++j)
			{
				float kerning = stbtt_GetCodepointKernAdvance(&font, i + FIRST_CODEPOINT, j + FIRST_CODEPOINT) * scale;
				this->advanceX[i * (NUM_CODEPOINTS + 1) + j] = advance + kerning;
			}
			float kerning = stbtt_GetCodepointKernAdvance(&font, i + FIRST_CODEPOINT, ' ') * scale;
			this->advanceX[i * (NUM_CODEPOINTS + 1) + NUM_CODEPOINTS] = advance + kerning;
		}

		uint32 currentPackSize = 256;

		int32 packResult;
		do
		{
			stbrp_context packingContext;
			std::vector<stbrp_node> nodes(currentPackSize);
			stbrp_init_target(&packingContext, currentPackSize, currentPackSize, nodes.data(), (int32)nodes.size());
			packResult = stbrp_pack_rects(&packingContext, packRects, NUM_CODEPOINTS);
			
			if (packResult)
			{
				uint8* bitmap = new uint8[currentPackSize * currentPackSize];
				memset(bitmap, 0, currentPackSize * currentPackSize);

				for (uint32 i = 0; i < NUM_CODEPOINTS; ++i)
				{
					font_glyph& g = glyphs[i];

					packRects[i].x += extraPadding;
					packRects[i].y += extraPadding;
					packRects[i].w -= extraPadding;
					packRects[i].h -= extraPadding;

					g.left = (float)packRects[i].x / (currentPackSize - 1);
					g.top = (float)packRects[i].y / (currentPackSize - 1);
					g.right = (float)(packRects[i].x + packRects[i].w - 1) / (currentPackSize - 1);
					g.bottom = (float)(packRects[i].y + packRects[i].h - 1) / (currentPackSize - 1);

					copyGlyphToAtlas(bitmap, currentPackSize, currentPackSize, cpBitmaps[i], packRects[i].x, packRects[i].y, g.width, g.height);
				}

				commandList->loadTextureFromMemory(atlas, bitmap, currentPackSize, currentPackSize, DXGI_FORMAT_R8_UNORM, texture_type_noncolor, true);
				delete[] bitmap;
			}

			currentPackSize *= 2;
		} while (!packResult);


		for (uint32 i = 0; i < NUM_CODEPOINTS; ++i)
		{
			stbtt_FreeSDF(cpBitmaps[i], nullptr);
		}
	}

	delete[] ttfBuffer;
	return true;
}
