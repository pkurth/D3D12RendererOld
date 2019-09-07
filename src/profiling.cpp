#include "pch.h"
#include "profiling.h"
#include "debug_gui.h"

#ifdef PROFILE

profile_event profileEvents[2][MAX_NUM_PROFILE_EVENTS];
std::atomic_uint64_t profileArrayAndEventIndex;
static uint32 lastFrameEventCount;



static uint64 performanceFrequency;


#define MAX_NUM_RECORDED_FRAMES			256
#define MAX_NUM_RECORDED_THREADS		16
#define MAX_RECORDED_CALLSTACK_DEPTH	128
#define MAX_NUM_PROFILE_BLOCKS			(MAX_NUM_RECORDED_FRAMES * MAX_NUM_RECORDED_THREADS * MAX_RECORDED_CALLSTACK_DEPTH)

struct profile_block
{
	profile_block* firstChild;
	profile_block* nextSibling;
	profile_block* parent;

	uint64 startClock;
	uint64 endClock;
	const char* info;
};

struct profile_frame
{
	uint64 startClock;
	uint64 endClock;
	uint64 globalFrameID;
	float timeInSeconds;

	profile_block* firstTopLevelBlockPerThread[MAX_NUM_RECORDED_THREADS];
};

struct profile_thread
{
	uint32 threadID;
	uint32 threadIndex;

	profile_block* callstack[MAX_RECORDED_CALLSTACK_DEPTH];
	uint32 callstackDepth;
};

static profile_frame recordedProfileFrames[MAX_NUM_RECORDED_FRAMES];
static uint32 currentProfileFrame = -1;
static uint32 highlightFrameIndex = -1;


static profile_thread recordedProfileThreads[MAX_NUM_RECORDED_THREADS];
static uint32 numProfileThreads;

static profile_block* allProfileBlocks = new profile_block[MAX_NUM_PROFILE_BLOCKS];
static uint32 nextFreeProfileBlock;

static profile_block* currentFrameLastTopLevelBlocks[MAX_NUM_RECORDED_THREADS];

static bool profilingPaused;

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
	uint32 index = numProfileThreads++;
	profile_thread& result = recordedProfileThreads[index];
	result.threadID = threadID;
	result.callstackDepth = 0;
	result.threadIndex = index;
	return result;
}

static void collateProfileEvents(profile_event* profileEvents, uint32 numProfileEvents)
{
	PROFILE_FUNCTION();

	profile_frame* frame = (currentProfileFrame != -1) ? (recordedProfileFrames + currentProfileFrame) : nullptr;

	for (uint32 eventIndex = 0; eventIndex < numProfileEvents; ++eventIndex)
	{
		profile_event& event = profileEvents[eventIndex];
		profile_thread& thread = getProfileThread(event.threadID);

		if (event.type == profile_event_frame_marker)
		{
			assert(thread.callstackDepth == 0);

			if (frame)
			{
				// End frame.
				assert(thread.callstackDepth == 0);
				assert(&thread == &recordedProfileThreads[0]);

				frame->endClock = event.clock;
				frame->timeInSeconds = ((float)(frame->endClock - frame->startClock) / (float)performanceFrequency);
			}

			// New frame.
			++currentProfileFrame;
			if (currentProfileFrame >= arraysize(recordedProfileFrames))
			{
				currentProfileFrame = 0;
			}
			profile_frame* newFrame = recordedProfileFrames + currentProfileFrame;
			
			newFrame->startClock = event.clock;
			newFrame->endClock = 0;
			newFrame->globalFrameID = event.frameID;

			for (uint32 threadIndex = 0; threadIndex < MAX_NUM_RECORDED_THREADS; ++threadIndex)
			{
				profile_block* prevFrameLastTopLevelBlock = currentFrameLastTopLevelBlocks[threadIndex];
				if (prevFrameLastTopLevelBlock && prevFrameLastTopLevelBlock->endClock == 0)
				{
					// This block still runs, so it becomes the first block of the next frame.
					newFrame->firstTopLevelBlockPerThread[threadIndex] = prevFrameLastTopLevelBlock;
				}
				else
				{
					newFrame->firstTopLevelBlockPerThread[threadIndex] = nullptr;
				}
			}

			frame = newFrame;
		}
		else if (event.type == profile_event_begin_block)
		{
			assert(thread.callstackDepth < arraysize(thread.callstack));

			uint32 profileBlockIndex = nextFreeProfileBlock++;
			if (nextFreeProfileBlock >= MAX_NUM_PROFILE_BLOCKS)
			{
				nextFreeProfileBlock = 0;
			}

			profile_block* newProfileBlock = allProfileBlocks + profileBlockIndex;
			newProfileBlock->startClock = event.clock;
			newProfileBlock->endClock = 0;
			newProfileBlock->info = event.info;
			newProfileBlock->firstChild = nullptr;
			newProfileBlock->nextSibling = nullptr;
			
			if (thread.callstackDepth > 0)
			{
				uint32 parent = thread.callstackDepth - 1;
				newProfileBlock->parent = thread.callstack[parent];
				if (!thread.callstack[parent]->firstChild)
				{
					thread.callstack[parent]->firstChild = newProfileBlock;
				}
				else
				{
					thread.callstack[thread.callstackDepth]->nextSibling = newProfileBlock;
				}
			}
			else
			{
				newProfileBlock->parent = nullptr;
				currentFrameLastTopLevelBlocks[thread.threadIndex] = newProfileBlock;

				if (!frame->firstTopLevelBlockPerThread[thread.threadIndex])
				{
					frame->firstTopLevelBlockPerThread[thread.threadIndex] = newProfileBlock;
				}
				else
				{
					thread.callstack[0]->nextSibling = newProfileBlock;
				}
			}

			thread.callstack[thread.callstackDepth++] = newProfileBlock;
		}
		else if (event.type == profile_event_end_block)
		{
			assert(thread.callstackDepth > 0);
			profile_block* block = thread.callstack[--thread.callstackDepth];
			block->endClock = event.clock;
		}
	}
}

static float measureSignedTime(uint64 frameStart, uint64 clock)
{
	if (clock >= frameStart)
	{
		return ((float)(clock - frameStart) / (float)performanceFrequency);
	}
	else
	{
		return -((float)(frameStart - clock) / (float)performanceFrequency);
	}
}

static uint32 currentDisplayCallDepth[MAX_NUM_RECORDED_THREADS];

static uint32 colorTable[] =
{
	color_32(255, 0, 0, 255),
	color_32(0, 255, 0, 255),
	color_32(0, 0, 255, 255),

	color_32(255, 0, 255, 255),
	color_32(255, 255, 0, 255),
	color_32(0, 255, 255, 255),

	color_32(255, 128, 0, 255),
	color_32(0, 255, 128, 255),
	color_32(128, 0, 255, 255),

	color_32(128, 255, 0, 255),
	color_32(0, 128, 255, 255),
	color_32(255, 0, 128, 255),
};

static void displayProfileInfo(debug_gui& gui)
{
	PROFILE_FUNCTION();

	const float chartHeight60FPS = 100.f;
	const float chartHeight30FPS = chartHeight60FPS * 2.f;

	const float barWidth = 3.f;
	const float barSpacing = 4.f;
	const float bottom = 300.f;
	const float leftOffset = 5.f;

	DEBUG_TAB(gui, "Profiling")
	{
		if (gui.toggle("Paused", profilingPaused))
		{
			if (profilingPaused)
			{
				for (uint32 i = 0; i < MAX_NUM_RECORDED_THREADS; ++i)
				{
					currentFrameLastTopLevelBlocks[i] = nullptr;
				}
			}
		}

		uint32 frameColor = color_32(255, 0, 0, 255);
		uint32 highlightFrameColor = color_32(255, 255, 0, 255);

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

			uint32 color = (frameIndex == highlightFrameIndex) ? highlightFrameColor : frameColor;

			if (gui.quadButton((uint64)frame, left, right, top, bottom, color))
			{
				highlightFrameIndex = frameIndex;
			}
		}

		gui.quad(leftOffset, MAX_NUM_RECORDED_FRAMES * barSpacing, bottom - chartHeight30FPS - 1, bottom - chartHeight30FPS, 0xFFFFFFFF);
		gui.quad(leftOffset, MAX_NUM_RECORDED_FRAMES * barSpacing, bottom - chartHeight60FPS - 1, bottom - chartHeight60FPS, 0xFFFFFFFF);

		gui.textAt(MAX_NUM_RECORDED_FRAMES * barSpacing + 3.f, bottom - chartHeight30FPS - 1, "33.3 ms");
		gui.textAt(MAX_NUM_RECORDED_FRAMES * barSpacing + 3.f, bottom - chartHeight60FPS - 1, "16.7 ms");

		if (highlightFrameIndex != -1)
		{
			const float topOffset = 400.f;
			const float barHeight = 25.f;
			const float barSpacing = 30.f;
			const float frameWidth60FPS = 400.f;
			const float frameWidth30FPS = frameWidth60FPS * 2.f;
			const float leftOffset = 100.f;

			bool scrollHandled = false;

			profile_frame* frame = recordedProfileFrames + highlightFrameIndex;
			if (frame->endClock != 0)
			{
				uint32 highestThreadIndex = 0;
				for (uint32 threadIndex = 0; threadIndex < MAX_NUM_RECORDED_THREADS; ++threadIndex)
				{
					float top = threadIndex * barSpacing + topOffset;
					float bottom = top + barHeight;

					uint32 callDepth = currentDisplayCallDepth[threadIndex];
					uint32 colorIndex = 0;

					for (profile_block* block = frame->firstTopLevelBlockPerThread[threadIndex]; 
						block && block->startClock < frame->endClock; 
						block = block->nextSibling)
					{
						profile_block* displayBlock = block;
						uint32 displayedCallDepth = 0;
						for (; displayedCallDepth < callDepth && displayBlock->firstChild; ++displayedCallDepth)
						{
							displayBlock = displayBlock->firstChild;
						}

						for (; displayBlock && displayBlock->startClock < frame->endClock; displayBlock = displayBlock->nextSibling)
						{
							float relStartTime = measureSignedTime(frame->startClock, displayBlock->startClock);
							float relEndTime = measureSignedTime(frame->startClock, displayBlock->endClock);

							float left = leftOffset + relStartTime / 0.0167f * frameWidth60FPS;
							float right = leftOffset + relEndTime / 0.0167f * frameWidth60FPS;


							auto [click, hover, scroll] = gui.interactableQuad((uint64)displayBlock, left, right, top, bottom, colorTable[colorIndex]);
							if (hover)
							{
								gui.textAtMouseF("%s: %f ms", displayBlock->info, (relEndTime - relStartTime) * 1000.f);
							}
							if (!scrollHandled)
							{
								if (scroll > 0.f && callDepth < MAX_RECORDED_CALLSTACK_DEPTH)
								{
									++callDepth;
									scrollHandled = true;
									break;
								}
								if (scroll < 0.f && callDepth > 0)
								{
									--callDepth;
									scrollHandled = true;
									break;
								}
							}

							++colorIndex;
							if (colorIndex >= arraysize(colorTable))
							{
								colorIndex = 0;
							}

							if (displayedCallDepth == 0)
							{
								break;
							}
						}
						highestThreadIndex = threadIndex;
					}

					currentDisplayCallDepth[threadIndex] = callDepth;
				}

				float top = topOffset - 30.f;
				float bottom = topOffset + (highestThreadIndex + 1) * barSpacing + 30.f;
				gui.quad(leftOffset, leftOffset + 1, top, bottom, 0xFFFFFFFF);
				gui.quad(leftOffset + frameWidth60FPS, leftOffset + frameWidth60FPS + 1, top, bottom, 0xFFFFFFFF);
				gui.quad(leftOffset + frameWidth30FPS, leftOffset + frameWidth30FPS + 1, top, bottom, 0xFFFFFFFF);

				gui.textAt(leftOffset, top, "0 ms");
				gui.textAt(leftOffset + frameWidth60FPS, top, "16.7 ms");
				gui.textAt(leftOffset + frameWidth30FPS, top, "33.3 ms");
			}
		}
	}
}

void processAndDisplayProfileEvents(uint64 currentFrameID, debug_gui& gui)
{
	PROFILE_FUNCTION();

	static bool performanceFrequencyQueried = QueryPerformanceFrequency((LARGE_INTEGER*)& performanceFrequency);

	uint64 currentArrayAndEventIndex = profileArrayAndEventIndex; // We are only interested in upper 32 bits, so don't worry about thread safety.
	uint32 arrayIndex = (uint32)(!(currentArrayAndEventIndex >> 32));
	uint32 eventCount = lastFrameEventCount;

	if (!profilingPaused)
	{
		collateProfileEvents(profileEvents[arrayIndex], eventCount);
	}

	if (currentProfileFrame >= 0)
	{
		displayProfileInfo(gui);
	}

	currentArrayAndEventIndex = profileArrayAndEventIndex.exchange((uint64)arrayIndex << 32);
	lastFrameEventCount = (uint32)(currentArrayAndEventIndex & 0xFFFFFFFF);
}

#endif
