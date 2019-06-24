#pragma once

#define NOMINMAX

#include <cstdint>

typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define arraysize(arr) (sizeof(arr) / sizeof(arr[0]))


#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

template <typename T>
constexpr T min(T a, T b)
{
	return (a < b) ? a : b;
}

template <typename T>
constexpr T max(T a, T b)
{
	return (a > b) ? a : b;
}

template<typename T>
constexpr T clamp(T val, T min, T max)
{
	return val < min ? min : val > max ? max : val;
}
