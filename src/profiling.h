#pragma once

#include "common.h"

#include <intrin.h>

#ifdef PROFILE

enum profile_event_type
{
	profile_event_frame_marker,
	profile_event_begin_block,
	profile_event_end_block,
};

struct profile_event
{
	uint64 clock;
	uint32 threadID;
	profile_event_type type;
	union
	{
		const char* info;
		uint64 frameID;
	};
};

static_assert(sizeof(profile_event) == 24, "Profile event struct is misaligned or something.");

#define MAX_NUM_PROFILE_EVENTS 16384
extern profile_event profileEvents[2][MAX_NUM_PROFILE_EVENTS];
extern std::atomic_uint64_t profileArrayAndEventIndex;

#define recordProfileEvent(type_, info_frameID) \
	uint64 arrayAndEventIndex = profileArrayAndEventIndex++; \
	uint32 eventIndex = (arrayAndEventIndex & 0xFFFFFFFF); \
	assert(eventIndex < MAX_NUM_PROFILE_EVENTS); \
	profile_event* event = profileEvents[arrayAndEventIndex >> 32] + eventIndex; \
	uint8* threadLocalStorage = (uint8*)__readgsqword(0x30); \
	event->threadID = *(uint32*)(threadLocalStorage + 0x48); \
	event->frameID = info_frameID; \
	event->type = type_; \
	QueryPerformanceCounter((LARGE_INTEGER*)&event->clock);

struct profile_block_recorder
{
	uint64 info_frameID;

	profile_block_recorder(const char* info)
	{
		info_frameID = (uint64)info;
		recordProfileEvent(profile_event_begin_block, info_frameID);
	}

	~profile_block_recorder()
	{
		recordProfileEvent(profile_event_end_block, info_frameID);
	}
};

#define PROFILE_INFO__(a, b, c, d) a b " | " c "[" #d "]"
#define PROFILE_INFO_(a, b, c, d) PROFILE_INFO__(a, b, c, d)
#define PROFILE_INFO(prefix) PROFILE_INFO_(prefix, __FUNCTION__, "", __LINE__)

#define PROFILE_BLOCK_(counter, prefix) profile_block_recorder COMPOSITE_VARNAME(PROFILE_BLOCK, counter)(PROFILE_INFO(prefix))

#define PROFILE_FUNCTION() PROFILE_BLOCK_(__COUNTER__, "")
#define PROFILE_BLOCK(name) PROFILE_BLOCK_(__COUNTER__, name ": ")
#define PROFILE_FRAME_MARKER(frameNum) { recordProfileEvent(profile_event_frame_marker, frameNum); }
#define PROFILE_INITIALIZATION() PROFILE_FRAME_MARKER(-1)

void processAndDisplayProfileEvents(uint64 currentFrameID, class debug_gui& gui);

#else
#define PROFILE_FUNCTION()
#define PROFILE_BLOCK(name)
#define PROFILE_FRAME_MARKER(frameNum)
#define PROFILE_INITIALIZATION()

#define processAndDisplayProfileEvents(...)

#endif

