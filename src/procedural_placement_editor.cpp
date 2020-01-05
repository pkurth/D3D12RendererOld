#include "pch.h"
#include "procedural_placement_editor.h"
#include "command_queue.h"


void procedural_placement_editor::initialize()
{
	mouseDown = false;
	brushRadius = 10.f;
	brushStrength = 1.f;

	registerMouseButtonDownCallback(BIND(mouseDownCallback));
	registerMouseButtonUpCallback(BIND(mouseUpCallback));
	registerMouseMoveCallback(BIND(mouseMoveCallback));
}

void procedural_placement_editor::update(const render_camera& camera, procedural_placement& placement, debug_gui& gui)
{
	DEBUG_TAB(gui, "Procedural placement")
	{
		DEBUG_GROUP(gui, "Brush")
		{
			gui.radio("Brush type", placementBrushNames, arraysize(placementBrushNames), (uint32&)brushType);
			gui.slider("Brush radius", brushRadius, 0.1f, 50.f);
			gui.slider("Brush strength", brushStrength, 0.f, 1.f);
		}
		gui.textF("Mouse position: %.3f, %.3f", mousePosition.x, mousePosition.y);

		if (mouseDown)
		{
			ray r = camera.getWorldSpaceRay(mousePosition.x, mousePosition.y);

			for (placement_tile& tile : placement.tiles)
			{
				vec2 corner0(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);
				vec2 corner1 = corner0 + vec2(PROCEDURAL_TILE_SIZE, PROCEDURAL_TILE_SIZE);

				vec4 plane(0.f, 1.f, 0.f, -tile.groundHeight);

				float t;
				if (r.intersectPlane(plane, t))
				{
					vec3 position = r.origin + t * r.direction;
					if (position.x >= corner0.x && position.x <= corner1.x &&
						position.z >= corner0.y && position.z <= corner1.y)
					{
						gui.textF("%.2f %.2f %.2f", position.x, position.y, position.z);

					}
				}

			}
		}
	}

}

bool procedural_placement_editor::mouseDownCallback(mouse_button_event event)
{
	mousePosition = vec2(event.relX, event.relY);
	if (event.button == mouse_left)
	{
		mouseDown = true;
	}
	return false;
}

bool procedural_placement_editor::mouseUpCallback(mouse_button_event event)
{
	mousePosition = vec2(event.relX, event.relY);
	if (event.button == mouse_left)
	{
		mouseDown = false;
	}
	return false;
}

bool procedural_placement_editor::mouseMoveCallback(mouse_move_event event)
{
	mousePosition = vec2(event.relX, event.relY);
	return false;
}
