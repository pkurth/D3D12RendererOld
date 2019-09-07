#pragma once

#include "common.h"
#include "font.h"
#include "math.h"
#include "command_list.h"
#include "platform.h"

struct gui_font_vertex
{
	vec2 position;
	vec2 uv;
	uint32 color;
};

struct gui_shape_vertex
{
	vec2 position;
	uint32 color;
};

#define DEBUG_GUI_TEXT_COLOR 0xFFFFFFFF
#define DEBUG_GUI_HOVERED_COLOR 0xFF7a7a7a

#define DEBUG_GUI_TAB_COLOR 0xFF7aFFFF
#define DEBUG_GUI_GROUP_COLOR 0xFF7aFF7a
#define DEBUG_GUI_BUTTON_COLOR 0xFF7a7aFF
#define DEBUG_GUI_TOGGLE_COLOR 0xFFFF7a7a


union color_32
{
	uint32 val;
	struct
	{
		uint8 r, g, b, a;
	};

	color_32(uint32 v)
	{
		val = v;
	}

	color_32(uint8 r, uint8 g, uint8 b, uint8 a)
	{
		val = (a << 24) | (b << 16) | (g << 8) | r;
	}

	operator uint32()
	{
		return val;
	}
};

struct debug_gui_interaction
{
	bool click;
	bool hover;
	float scroll;
};

class debug_gui
{
public:
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, D3D12_RT_FORMAT_ARRAY rtvFormats);

	bool tab(const char* name);

	void text(const char* text);
	void textF(const char* format, ...);
	void textV(const char* format, va_list arg);

	void value(const char* name, bool v);
	void value(const char* name, int32 v);
	void value(const char* name, uint32 v);
	void value(const char* name, float v);

	bool toggle(const char* name, bool& v);
	bool button(const char* name);



	void quad(float left, float right, float top, float bottom, uint32 color);
	bool quadButton(uint64 guid, float left, float right, float top, float bottom, uint32 color, const char* formatOnHover = nullptr, ...);
	bool quadHover(float left, float right, float top, float bottom, uint32 color);
	float quadScroll(float left, float right, float top, float bottom, uint32 color);
	debug_gui_interaction interactableQuad(uint64 guid, float left, float right, float top, float bottom, uint32 color);
	void textAt(float x, float y, const char* text);
	void textAtF(float x, float y, const char* format, ...);
	void textAtV(float x, float y, const char* format, va_list arg);

	void textAtMouse(const char* text);
	void textAtMouseF(const char* format, ...);
	void textAtMouseV(const char* format, va_list arg);


	void render(dx_command_list* commandList, const D3D12_VIEWPORT& viewport);

	bool mouseDownCallback(mouse_button_event event);
	bool mouseUpCallback(mouse_button_event event);
	bool mouseMoveCallback(mouse_move_event event);
	bool mouseScrollCallback(mouse_scroll_event event);

private:
	enum event_type
	{
		event_type_none,
		event_type_down,
		event_type_up
	};

	friend struct debug_group_internal;

	void resizeIndexBuffer(dx_command_list* commandList, uint32 numQuads);

	float getCursorX();

	struct text_analysis
	{
		uint32 numGlyphs;
		vec2 topLeft;
		vec2 bottomRight;
	};

	uint64 hashLabel(const char* name);

	text_analysis analyzeText(const char* text, float size = 1.f);
	void textInternal(const char* text, uint32 color = DEBUG_GUI_TEXT_COLOR, float size = 1.f);
	void textInternalF(const char* format, uint32 color = DEBUG_GUI_TEXT_COLOR, float size = 1.f, ...);
	void textInternalV(const char* format, va_list arg, uint32 color = DEBUG_GUI_TEXT_COLOR, float size = 1.f);
	void textInternalAt(float x, float y, const char* text, uint32 color = DEBUG_GUI_TEXT_COLOR, float size = 1.f);

	uint32 handleButtonPress(uint64 id, vec2 topLeft, vec2 bottomRight);
	uint32 handleButtonPress(uint64 id, const char* name, float size);
	bool buttonInternal(uint64 id, const char* name, uint32 color, float size = 1.f, bool isTab = false);
	bool buttonInternalF(uint64 id, const char* name, uint32 color, float size = 1.f, ...);

	bool beginGroupInternal(const char* name, bool& isOpen);
	void endGroupInternal();


	float textHeight;
	dx_font font;


	float cursorY;
	uint32 level;

	std::vector<uint64> allTabsSeenThisFrame;

	float tabAdvance;
	uint32 numActiveTabs;
	uint64 openTab;
	uint64 firstTab;
	bool openTabSeenThisFrame;

	std::vector<gui_font_vertex> currentFontVertices;
	std::vector<gui_shape_vertex> currentShapeVertices;

	ComPtr<ID3D12Device2> device;
	dx_index_buffer indexBuffer;

	ComPtr<ID3D12PipelineState> fontPipelineState;
	dx_root_signature fontRootSignature;

	ComPtr<ID3D12PipelineState> shapePipelineState;
	dx_root_signature shapeRootSignature;


	vec2 mousePosition;
	float mouseScroll;
	event_type lastEventType;

	uint64 activeID;
};


struct debug_group_internal
{
	bool initialize(debug_gui& gui, const char* name, bool& isOpen)
	{
		if (this->gui)
		{
			return false;
		}
		if (gui.beginGroupInternal(name, isOpen))
		{
			this->gui = &gui;
		}
		return isOpen;
	}

	~debug_group_internal()
	{
		if (gui)
		{
			gui->endGroupInternal();
		}
	}

	debug_gui* gui = 0;
};

// This for loop will never run more than once. The loop makes the group variable live until the end of the block.
#define DEBUG_GROUP_(gui, name, boolname) \
	static bool boolname = true; \
	for (debug_group_internal group; group.initialize(gui, name, boolname);)

#define DEBUG_GROUP(gui, name) DEBUG_GROUP_(gui, name, COMPOSITE_VARNAME(isOpen, __LINE__))


#define DEBUG_TAB(gui, name) if (gui.tab(name))

