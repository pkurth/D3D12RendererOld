#pragma once

#include "input.h"
#include <functional>

#define NUM_BUFFERED_FRAMES 2

void flushApplication();
void registerKeyDownCallback(std::function<bool(keyboard_event)> func);
void registerKeyUpCallback(std::function<bool(keyboard_event)> func);
void registerCharacterCallback(std::function<bool(character_event)> func);
void registerMouseButtonDownCallback(std::function<bool(mouse_button_event)> func);
void registerMouseButtonUpCallback(std::function<bool(mouse_button_event)> func);
void registerMouseMoveCallback(std::function<bool(mouse_move_event)> func);
void registerMouseScrollCallback(std::function<bool(mouse_scroll_event)> func);

#define BIND(func) std::bind(&std::remove_reference<decltype(*this)>::type::func, this, std::placeholders::_1)


