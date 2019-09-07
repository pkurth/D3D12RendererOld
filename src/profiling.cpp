#include "pch.h"
#include "profiling.h"
#include "debug_gui.h"

#ifdef PROFILE

profile_event profileEvents[2][MAX_NUM_PROFILE_EVENTS];
std::atomic_uint64_t profileArrayAndEventIndex;
static uint32 lastFrameEventCount;





#define MAX_NUM_RECORDED_FRAMES			256
#define MAX_NUM_RECORDED_THREADS		16
#define MAX_RECORDED_CALLSTACK_DEPTH	128

struct profile_frame
{
	uint64 startClock;
	uint64 endClock;
	uint64 globalFrameID;
	float timeInSeconds;
};

struct profile_thread
{
	uint32 threadID;

	uint64 callstack[MAX_RECORDED_CALLSTACK_DEPTH];
	uint32 callstackDepth;
};

static profile_frame recordedProfileFrames[MAX_NUM_RECORDED_FRAMES];
static uint32 currentProfileFrame = -1;

static profile_thread recordedProfileThreads[MAX_NUM_RECORDED_THREADS];
static uint32 numProfileThreads;


static profile_thread& getProfileThread(uint32 threadID)
{
	for (uint32 i = 0; i < numProfileThreads; ++i)
	{
		if (recordedProfileThreads[i].threadID == threadID)
		{
			return recordedProfileThreads[i];
		}
	}
	assert(numProfileThreads < arraysize(recordedProfileThreads));
	profile_thread& result = recordedProfileThreads[numProfileThreads++];
	result.threadID = threadID;
	result.callstackDepth = 0;
	return result;
}

static void collateProfileEvents(profile_event* profileEvents, uint32 numProfileEvents)
{
	uint64 performanceFrequency;
	QueryPerformanceFrequency((LARGE_INTEGER*)&performanceFrequency);

	profile_frame* frame = recordedProfileFrames + currentProfileFrame;

	for (uint32 eventIndex = 0; eventIndex < numProfileEvents; ++eventIndex)
	{
		profile_event& event = profileEvents[eventIndex];
		profile_thread& thread = getProfileThread(event.threadID);

		if (event.type == profile_event_begin_frame)
		{
			++currentProfileFrame;
			if (currentProfileFrame >= arraysize(recordedProfileFrames))
			{
				currentProfileFrame = 0;
			}
			frame = recordedProfileFrames + currentProfileFrame;
			
			frame->startClock = event.clock;
			frame->endClock = 0;
			frame->globalFrameID = event.frameID;

			assert(thread.callstackDepth == 0);
			thread.callstack[thread.callstackDepth++] = (uint64)-1;
		}
		else if (event.type == profile_event_begin_block)
		{
			assert(thread.callstackDepth < arraysize(thread.callstack));
			thread.callstack[thread.callstackDepth++] = (uint64)event.info;
		}
		else if (event.type == profile_event_end_block)
		{
			assert(thread.callstackDepth > 0);
			uint64 beginBlockInfo = thread.callstack[--thread.callstackDepth];
			
			if (beginBlockInfo == -1)
			{
				// End frame.
				assert(thread.callstackDepth == 0);
				assert(&thread == &recordedProfileThreads[0]);
				assert(frame->globalFrameID == event.frameID);

				frame->endClock = event.clock;
				frame->timeInSeconds = ((float)(frame->endClock - frame->startClock) / (float)performanceFrequency);
			}
		}
	}
}

static void displayProfileInfo(debug_gui& gui)
{
	const float chartHeight60FPS = 100.f;
	const float chartHeight30FPS = chartHeight60FPS * 2.f;

	const float barWidth = 3.f;
	const float barSpacing = 4.f;
	const float bottom = 400.f;
	const float leftOffset = 5.f;

	DEBUG_TAB(gui, "Profiling")
	{
		for (uint32 frameIndex = 0; frameIndex < MAX_NUM_RECORDED_FRAMES; ++frameIndex)
		{
			profile_frame* frame = recordedProfileFrames + frameIndex;
			if (frame->endClock == 0)
			{
				continue;
			}

			float height = frame->timeInSeconds / 0.0167f * chartHeight60FPS;
			float left = frameIndex * barSpacing + leftOffset;
			float right = left + barWidth;
			float top = bottom - height;

			gui.quad(left, right, top, bottom, color_32(255, 0, 0, 255));
		}

		gui.quad(leftOffset, MAX_NUM_RECORDED_FRAMES * barSpacing, bottom - chartHeight30FPS - 1, bottom - chartHeight30FPS, 0xFFFFFFFF);
		gui.quad(leftOffset, MAX_NUM_RECORDED_FRAMES * barSpacing, bottom - chartHeight60FPS - 1, bottom - chartHeight60FPS, 0xFFFFFFFF);

		gui.textAt(MAX_NUM_RECORDED_FRAMES * barSpacing + 3.f, bottom - chartHeight30FPS - 1, "33.3 ms");
		gui.textAt(MAX_NUM_RECORDED_FRAMES * barSpacing + 3.f, bottom - chartHeight60FPS - 1, "16.7 ms");
	}
}

void processAndDisplayProfileEvents(uint64 currentFrameID, debug_gui& gui)
{
	uint64 currentArrayAndEventIndex = profileArrayAndEventIndex; // We are only interested in upper 32 bits, so don't worry about thread safety.
	uint32 arrayIndex = (uint32)(!(currentArrayAndEventIndex >> 32));
	uint32 eventCount = lastFrameEventCount;

	collateProfileEvents(profileEvents[arrayIndex], eventCount);
	
	if (currentProfileFrame >= 0)
	{
		displayProfileInfo(gui);
	}

	currentArrayAndEventIndex = profileArrayAndEventIndex.exchange((uint64)arrayIndex << 32);
	lastFrameEventCount = (uint32)(currentArrayAndEventIndex & 0xFFFFFFFF);
}

#endif
