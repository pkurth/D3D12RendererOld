#include "pch.h"
#include "debug_gui.h"

void debug_gui::initialize(dx_command_list* commandList, dx_font& font)
{
	resizeIndexBuffer(commandList, 2048);
	yOffset = 0;
	level = 0;
	textHeight = 30.f;
	this->font = &font;
}

void debug_gui::resizeIndexBuffer(dx_command_list* commandList, uint32 numQuads)
{
	uint16* indices = new uint16[numQuads * 6];
	for (uint32 i = 0; i < numQuads; ++i)
	{
		indices[i * 6 + 0] = i * 4;
		indices[i * 6 + 1] = i * 4 + 1;
		indices[i * 6 + 2] = i * 4 + 2;
		indices[i * 6 + 3] = i * 4 + 1;
		indices[i * 6 + 4] = i * 4 + 3;
		indices[i * 6 + 5] = i * 4 + 2;
	}

	commandList->createIndexBuffer(indices, numQuads * 6);
}

void debug_gui::beginGroup(const char* name)
{
	text(name);
	++level;
}

void debug_gui::endGroup()
{
	assert(level > 0);
	--level;
}

void debug_gui::text(const char* format, va_list arg)
{
	char text[2048];
	uint32 N = vsnprintf(text, sizeof(text), format, arg);
	uint32 numCharacters = min<uint32>(N, sizeof(text));

	currentVertices.resize(currentVertices.size() + numCharacters);
	uint32 currentIndex = (uint32)currentVertices.size();

	float cursorX = 0.f;
	float cursorY = yOffset * font->height * 1.2f;
	float scale = textHeight / font->height;

	uint32 color = 0xFFFFFFFF;

	uint32 index = 0;
	while (text[index])
	{
		char c = text[index];
		if (c < FIRST_CODEPOINT || c > LAST_CODEPOINT)
		{
			c = '?';
		}

		uint32 glyphID = c - FIRST_CODEPOINT;

		font_glyph& glyph = font->glyphs[glyphID];

		float xStart = cursorX + glyph.offsetX * scale;
		float yStart = cursorY + glyph.offsetY * scale;

		float gWidth = glyph.width * scale;
		float gHeight = glyph.height * scale;

		currentVertices[currentIndex] = { vec2(xStart, yStart), vec2(glyph.left, glyph.top), color };
		currentVertices[currentIndex] = { vec2(xStart + gWidth, yStart), vec2(glyph.right, glyph.top), color };
		currentVertices[currentIndex] = { vec2(xStart, yStart + gHeight), vec2(glyph.left, glyph.bottom), color };
		currentVertices[currentIndex] = { vec2(xStart + gWidth, yStart + gHeight), vec2(glyph.right, glyph.bottom), color };

		char nextC = text[index + 1];
		if (nextC)
		{
			uint32 nextGlyphID = nextC - FIRST_CODEPOINT;
			cursorX += font->advanceX[glyphID * (uint64)NUM_CODEPOINTS + nextGlyphID] * scale;
		}

		++index;
		++currentIndex;
	}

	++yOffset;
}

void debug_gui::text(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	text(format, arg);
	va_end(arg);

}

void debug_gui::value(const char* name, float v)
{
	text("%s: %f", name, v);
}

void debug_gui::render(dx_command_list* commandList)
{
	// TODO: Shader.

	assert(level == 0);

	if ((uint32)currentVertices.size() > indexBuffer.numIndices)
	{
		resizeIndexBuffer(commandList, (uint32)currentVertices.size());
	}

	D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(currentVertices.data(), (uint32)currentVertices.size());

	commandList->setVertexBuffer(0, tmpVertexBuffer);
	commandList->setIndexBuffer(indexBuffer);

	commandList->drawIndexed(indexBuffer.numIndices, 1, 0, 0, 0);

	currentVertices.clear();
	yOffset = 0;
	level = 0;
}
