#pragma once

#define NOMINMAX

#include <cstdint>
#include <cassert>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)
#define GB(x) (x * 1024 * 1024 * 1024)

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

template<typename T>
inline T alignTo(T currentOffset, T alignment)
{
	assert(alignment > 0);
	assert((alignment & (alignment - 1)) == 0);

	T mask = alignment - 1;
	T misalignment = currentOffset & mask;
	T adjustment = (alignment - misalignment) & mask; // & mask ensures that a misalignment of 0 does not lead to an adjustment of 'alignment'
	return currentOffset + adjustment;
}

inline void* alignTo(void* currentAddress, uint64 alignment)
{
	return (void*)alignTo((uint64)currentAddress, alignment);
}
