#pragma once

#include "common.h"
#include "font.h"
#include "math.h"
#include "command_list.h"
#include "platform.h"

struct gui_vertex
{
	vec2 position;
	vec2 uv;
	uint32 color;
};

class debug_gui
{
public:
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, dx_font& font, D3D12_RT_FORMAT_ARRAY rtvFormats);

	void beginGroup(const char* name);
	void endGroup();

	void text(const char* format, va_list arg);
	void text(const char* format, ...);

	void value(const char* name, float v);

	void render(dx_command_list* commandList, const D3D12_VIEWPORT& viewport);

	bool mouseCallback(mouse_input_event event);

private:
	void resizeIndexBuffer(dx_command_list* commandList, uint32 numQuads);


	uint32 yOffset;
	uint32 level;
	float textHeight;
	dx_font* font;

	dx_texture whiteTexture;

	std::vector<gui_vertex> currentVertices;

	dx_index_buffer indexBuffer;

	ComPtr<ID3D12PipelineState> pipelineState;
	dx_root_signature rootSignature;
};

