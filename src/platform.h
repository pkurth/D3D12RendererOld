#pragma once

#include "input.h"
#include <functional>

void flushApplication();
void registerKeyboardCallback(std::function<bool(key_input_event)> func);
void registerMouseCallback(std::function<bool(mouse_input_event)> func);

#define BIND(func) std::bind(&std::remove_reference<decltype(*this)>::type::func, this, std::placeholders::_1)

