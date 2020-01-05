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

template <typename T>
inline void append(std::vector<T>& appendHere, const std::vector<T>& appendMe)
{
	appendHere.insert(appendHere.end(), appendMe.begin(), appendMe.end());
}

inline std::wstring stringToWString(const std::string& s)
{
	return std::wstring(s.begin(), s.end());
}

namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
	template <typename T>
	inline void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
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


#define COMPOSITE_VARNAME_(a, b) a##b
#define COMPOSITE_VARNAME(a, b) COMPOSITE_VARNAME_(a, b)



#define EXPAND(a) a

#define DECLARE_ENUM_VALUE(enum_value, enum_name) enum_value,
#define DECLARE_ENUM_NAME(enum_value, enum_name) enum_name,

#define FOR_EACH_MEMBER_1(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name))
#define FOR_EACH_MEMBER_2(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_1(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_3(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_2(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_4(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_3(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_5(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_4(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_6(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_5(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_7(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_6(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_8(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_7(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_9(macro,  enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_8(macro, __VA_ARGS__)))
#define FOR_EACH_MEMBER_10(macro, enum_value, enum_name, ...)	EXPAND(macro(enum_value, enum_name)	EXPAND(FOR_EACH_MEMBER_9(macro, __VA_ARGS__)))


#define GET_EVERY_SECOND_MACRO_(_1,__1,_2,__2,_3,__3,_4,__4,_5,__5,_6,__6,_7,__7,_8,__8,_9,__9,_10,__10,num,...) num
#define GET_EVERY_SECOND_MACRO(...) EXPAND(GET_EVERY_SECOND_MACRO_(__VA_ARGS__,INVALID,FOR_EACH_MEMBER_9,INVALID,FOR_EACH_MEMBER_8,INVALID,FOR_EACH_MEMBER_7,INVALID,FOR_EACH_MEMBER_6,INVALID,FOR_EACH_MEMBER_5,INVALID,FOR_EACH_MEMBER_4,INVALID,FOR_EACH_MEMBER_3,INVALID,FOR_EACH_MEMBER_2,INVALID,FOR_EACH_MEMBER_1,INVALID,INVALID))



#define STRINGIFY_ENUM(name, ...) \
	EXPAND(GET_EVERY_SECOND_MACRO(__VA_ARGS__)(DECLARE_ENUM_VALUE, __VA_ARGS__)) }; \
	static const char* name[] = { \
		EXPAND(GET_EVERY_SECOND_MACRO(__VA_ARGS__)(DECLARE_ENUM_NAME, __VA_ARGS__)) \
	};


