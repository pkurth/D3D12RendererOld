#include "pch.h"
#include "math.h"

const mat4 mat4::identity = { 1.f, 0.f, 0.f, 0.f,
							0.f, 1.f, 0.f, 0.f,
							0.f, 0.f, 1.f, 0.f,
							0.f, 0.f, 0.f, 1.f };

const quat quat::identity = { 0.f, 0.f, 0.f, 1.f };

const vec3 vec3::right = { 1.f, 0.f, 0.f };
const vec3 vec3::up = { 0.f, 1.f, 0.f };
const vec3 vec3::forward = { 0.f, 0.f, -1.f };

