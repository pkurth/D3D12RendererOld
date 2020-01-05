#pragma once

#include "debug_gui.h"
#include "procedural_placement.h"
#include "platform.h"

STRINGIFY_ENUM(placementBrushNames, 
enum placement_brush_type
{
	placement_brush_add,		"Add",
	placement_brush_subtract,	"Subtract",
};
)

struct procedural_placement_editor
{
	void initialize();

	void update(const render_camera& camera, procedural_placement& placement, debug_gui& gui);

	bool mouseDownCallback(mouse_button_event event);
	bool mouseUpCallback(mouse_button_event event);
	bool mouseMoveCallback(mouse_move_event event);


	placement_brush_type brushType = placement_brush_add;
	float brushRadius;
	float brushStrength;

	vec2 mousePosition;
	bool mouseDown;
};
