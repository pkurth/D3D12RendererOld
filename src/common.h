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


enum color_depth
{
	color_depth_8,
	color_depth_10,
};

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
constexpr T clamp01(T val)
{
	return clamp(val, T(0), T(1));
}

template <typename T>
inline T alignToWithMask(T value, size_t mask)
{
	return (T)(((size_t)value + mask) & ~mask);
}

template <typename T>
inline T alignTo(T value, size_t alignment)
{
	return alignToWithMask(value, alignment - 1);
}

inline void* alignTo(void* currentAddress, uint64 alignment)
{
	return (void*)alignTo((uint64)currentAddress, alignment);
}

#define defineHasMember(member_name)                                         \
    template <typename T>                                                      \
    class has_member_##member_name                                             \
    {                                                                          \
        typedef char yes_type;                                                 \
        typedef long no_type;                                                  \
        template <typename U> static yes_type test(decltype(&U::member_name)); \
        template <typename U> static no_type  test(...);                       \
    public:                                                                    \
        static constexpr bool value = sizeof(test<T>(0)) == sizeof(yes_type);  \
    }

#define hasMember(class_, member_name)  has_member_##member_name<class_>::value
