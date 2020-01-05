#pragma once

#include "debug_gui.h"
#include "procedural_placement.h"
#include "platform.h"

#define PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_MODEL	 0
#define PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_CB	 1
#define PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_TEX	 2


STRINGIFY_ENUM(placementBrushNames, 
enum placement_brush_type
{
	placement_brush_add,		"Add",
	placement_brush_subtract,	"Subtract",
	placement_brush_min,		"Min",
	placement_brush_max,		"Max",

	placement_brush_count,		"Count",
};
)

struct procedural_placement_editor
{
	void initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget);

	void update(dx_command_list* commandList, const render_camera& camera, procedural_placement& placement, debug_gui& gui);

	bool mouseDownCallback(mouse_button_event event);
	bool mouseUpCallback(mouse_button_event event);
	bool mouseMoveCallback(mouse_move_event event);


	placement_brush_type brushType = placement_brush_add;
	float brushRadius = 10.f;
	float brushStrength = 1.f;
	float brushHardness = 0.8f;
	uint32 densityMapIndex = 0;

	vec2 mousePosition;
	bool mouseDown;

	dx_render_target densityRT;

	ComPtr<ID3D12PipelineState> visualizeDensityPipelineState;
	dx_root_signature visualizeDensityRootSignature;

	ComPtr<ID3D12PipelineState> applyBrushPipelineState[placement_brush_count];
	dx_root_signature applyBrushRootSignature;
};
