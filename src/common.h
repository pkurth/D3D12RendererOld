#pragma once

#include <cstdint>

typedef uint32_t uint32;
typedef uint64_t uint64;

#define arraysize(arr) (sizeof(arr) / sizeof(arr[0]))

template <typename T>
T min(T a, T b)
{
	return (a < b) ? a : b;
}

template <typename T>
T max(T a, T b)
{
	return (a > b) ? a : b;
}
