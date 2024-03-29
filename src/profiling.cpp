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

struct profile_display_state
{
	float leftOffset;
	float frameWidth60FPS;
	float barHeight;
	uint32 colorIndex;
	float mouseHoverX;
};

static uint32 displayProfileBlock(profile_display_state& state, debug_gui& gui, profile_frame* frame, 
	profile_block* topLevelBlock, float top)
{
	uint32 result = 0;

	float bottom = top + state.barHeight;

	if (topLevelBlock)
	{
		result += 1;
	}

	for (profile_block* block = topLevelBlock; block; block = block->nextSibling)
	{
		float relStartTime = measureSignedTime(frame->startClock, block->startClock);
		float relEndTime = measureSignedTime(frame->startClock, block->endClock);

		float left = state.leftOffset + relStartTime / 0.0167f * state.frameWidth60FPS;
		float right = state.leftOffset + relEndTime / 0.0167f * state.frameWidth60FPS;

		if (gui.quadHover(left, right, top, bottom, colorTable[state.colorIndex]))
		{
			gui.textAtMouseF("%s: %f ms", block->info, (relEndTime - relStartTime) * 1000.f);
			state.mouseHoverX = gui.mousePosition.x;
		}

		++state.colorIndex;
		if (state.colorIndex >= arraysize(colorTable))
		{
			state.colorIndex = 0;
		}

		// Display children.
		result += displayProfileBlock(state, gui, frame, block->firstChild, bottom);
	}

	return result;
}

struct profile_block_statistics
{
	uint32 numCalls;
	float totalDuration;
	float averageDuration;
};

static void accumulateTimings(profile_block* topLevelBlock, std::unordered_map<const char*, profile_block_statistics>& outTimings)
{
	for (profile_block* block = topLevelBlock; block; block = block->nextSibling)
	{
		float duration = ((float)(block->endClock - block->startClock) / (float)performanceFrequency);

		profile_block_statistics& stat = outTimings[block->info];
		++stat.numCalls;
		stat.totalDuration += duration;

		accumulateTimings(block->firstChild, outTimings);
	}
}

static std::unordered_map<const char*, profile_block_statistics> accumulateTimings(profile_frame* frame)
{
	std::unordered_map<const char*, profile_block_statistics> timings;

	for (uint32 threadIndex = 0; threadIndex < MAX_NUM_RECORDED_THREADS; ++threadIndex)
	{
		profile_block* topLevelBlock = frame->firstTopLevelBlockPerThread[threadIndex];

		accumulateTimings(topLevelBlock, timings);
	}

	for (auto& it : timings)
	{
		it.second.averageDuration = it.second.totalDuration / it.second.numCalls;
	}

	return timings;
}

static std::unordered_map<const char*, profile_block_statistics> selectedFrameAccumulatedTimings;

static float normalFrameWidth60FPS = 500.f;
static float initializationFrameWidth60FPS = 2.f;

static float normalCallstackLeftOffset = 150.f;
static float initializationCallstackLeftOffset = 150.f;

static float frameWidth60FPS;
static float callstackLeftOffset;

static uint32 interactionGUID = 0;
static float scrollAnchor;
static bool scrolling;

static profile_display_mode displayMode;

static void displayProfileInfo(debug_gui& gui)
{
	PROFILE_FUNCTION();

	const float chartHeight60FPS = 100.f;
	const float chartHeight30FPS = chartHeight60FPS * 2.f;

	const float barWidth = 3.f;
	const float barSpacing = 4.f;
	const float bottom = 400.f;
	const float barLeftOffset = 5.f;

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
		gui.radio("Display mode", displayModeNames, profile_display_mode_count, (uint32&)displayMode);

		uint32 initColor = color_32(0, 255, 0, 255);
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
			float left = frameIndex * barSpacing + barLeftOffset;
			float right = left + barWidth;
			float top = bottom - height;

			uint32 color = (frame->globalFrameID == -1) ? initColor : frameColor;
			color = (frameIndex == highlightFrameIndex) ? highlightFrameColor : color;

			if (frame->globalFrameID == -1)
			{
				if (gui.quadButton((uint64)frame, left, right, top, bottom, color, "Initialization"))
				{
					highlightFrameIndex = frameIndex;
					frameWidth60FPS = initializationFrameWidth60FPS;
					callstackLeftOffset = initializationCallstackLeftOffset;

					selectedFrameAccumulatedTimings = accumulateTimings(frame);
				}
			}
			else
			{
				if (gui.quadButton((uint64)frame, left, right, top, bottom, color, "Frame %llu (%f ms)", frame->globalFrameID, frame->timeInSeconds * 1000.f))
				{
					highlightFrameIndex = frameIndex;
					frameWidth60FPS = normalFrameWidth60FPS;
					callstackLeftOffset = normalCallstackLeftOffset;

					selectedFrameAccumulatedTimings = accumulateTimings(frame);
				}
			}
		}

		gui.quad(barLeftOffset, MAX_NUM_RECORDED_FRAMES * barSpacing, bottom - chartHeight30FPS - 1, bottom - chartHeight30FPS, 0xFFFFFFFF);
		gui.quad(barLeftOffset, MAX_NUM_RECORDED_FRAMES * barSpacing, bottom - chartHeight60FPS - 1, bottom - chartHeight60FPS, 0xFFFFFFFF);

		gui.textAt(MAX_NUM_RECORDED_FRAMES * barSpacing + 3.f, bottom - chartHeight30FPS - 1, 0xFFFFFFFF, "33.3 ms");
		gui.textAt(MAX_NUM_RECORDED_FRAMES * barSpacing + 3.f, bottom - chartHeight60FPS - 1, 0xFFFFFFFF, "16.7 ms");

		if (highlightFrameIndex != -1)
		{
			float topOffset = 520.f;
			float barHeight = 25.f;
			float barSpacing = 30.f;
			float frameWidth30FPS = frameWidth60FPS * 2.f;

			profile_frame* frame = recordedProfileFrames + highlightFrameIndex;
			if (frame->endClock != 0)
			{
				if (frame->globalFrameID == -1)
				{
					gui.textAtF(callstackLeftOffset, topOffset - 60, 0xFFFFFFFF, "Initialization (%fs)", frame->timeInSeconds);
				}
				else
				{
					gui.textAtF(callstackLeftOffset, topOffset - 60, 0xFFFFFFFF, "Frame %llu (%f ms)", frame->globalFrameID, frame->timeInSeconds * 1000.f);
				}

				if (displayMode == profile_display_callstack)
				{
					profile_display_state state;
					state.colorIndex = 0;
					state.frameWidth60FPS = frameWidth60FPS;
					state.leftOffset = callstackLeftOffset;
					state.mouseHoverX = -1.f;
					state.barHeight = barHeight;


					// Display call stacks.
					uint32 currentLane = 0;
					for (uint32 threadIndex = 0; threadIndex < MAX_NUM_RECORDED_THREADS; ++threadIndex)
					{
						float top = currentLane * barSpacing + topOffset;
						uint32 numLanesInThread = displayProfileBlock(state, gui, frame, frame->firstTopLevelBlockPerThread[threadIndex], top);
						currentLane += numLanesInThread;

						if (numLanesInThread)
						{
							gui.textAtF(callstackLeftOffset - 100.f, top + barHeight - 3, 0xFFFFFFFF, "Thread %u", threadIndex);
							top += numLanesInThread * barHeight + 2;
							gui.quad(callstackLeftOffset - 100.f, callstackLeftOffset + frameWidth30FPS, top, top + 1, 0xFFFFFFFF);
						}
					}

					// Display millisecond spacings.
					float top = topOffset - 30.f;
					float bottom = topOffset + currentLane * barSpacing + 30.f;

					if (frame->globalFrameID != -1)
					{
						gui.quad(callstackLeftOffset, callstackLeftOffset + 1, top, bottom, 0xFFFFFFFF);
						gui.quad(callstackLeftOffset + frameWidth60FPS, callstackLeftOffset + frameWidth60FPS + 1, top, bottom, 0xFFFFFFFF);
						gui.quad(callstackLeftOffset + frameWidth30FPS, callstackLeftOffset + frameWidth30FPS + 1, top, bottom, 0xFFFFFFFF);

						gui.textAt(callstackLeftOffset, top, 0xFFFFFFFF, "0 ms");
						gui.textAt(callstackLeftOffset + frameWidth60FPS, top, 0xFFFFFFFF, "16.7 ms");
						gui.textAt(callstackLeftOffset + frameWidth30FPS, top, 0xFFFFFFFF, "33.3 ms");

						float millisecondSpacing = frameWidth30FPS / 33.3f;
						for (uint32 i = 1; i <= 33; ++i)
						{
							uint32 color = (i % 5 == 0) ? 0xEAFFFFFF : 0x7AFFFFFF;
							gui.quad(callstackLeftOffset + i * millisecondSpacing, callstackLeftOffset + i * millisecondSpacing + 1.f, top, bottom, color);
						}
					}
					else
					{
						float secondSpacing = frameWidth30FPS / 33.3f * 1000.f;
						uint32 lengthInSeconds = (uint32)ceil(frame->timeInSeconds);
						for (uint32 i = 0; i < lengthInSeconds; ++i)
						{
							uint32 color = (i % 5 == 0) ? 0xEAFFFFFF : 0x7AFFFFFF;
							gui.quad(callstackLeftOffset + i * secondSpacing, callstackLeftOffset + i * secondSpacing + 1.f, top, bottom, color);
							gui.textAtF(callstackLeftOffset + i * secondSpacing, top, 0xFFFFFFFF, "%us", i);
						}
					}

					if (state.mouseHoverX > 0.f)
					{
						gui.quad(state.mouseHoverX, state.mouseHoverX + 1.f, top, bottom, color_32(255, 255, 0, 255));
						float time = (state.mouseHoverX - callstackLeftOffset) / frameWidth30FPS * 33.3f;
						gui.textAtF(state.mouseHoverX, top, color_32(255, 255, 0, 255), "%.3f ms", time);
					}

					debug_gui_interaction interaction = gui.interactableQuad((uint64)&interactionGUID, callstackLeftOffset, 10000.f, topOffset, bottom, 0x0);
					if (interaction.scroll != 0.f)
					{
						float oldWidth = frameWidth60FPS;
						float oldRelMouseX = inverseLerp(callstackLeftOffset, callstackLeftOffset + frameWidth60FPS, gui.mousePosition.x);
						frameWidth60FPS += interaction.scroll * 60.f;

						if (frame->globalFrameID == -1)
						{
							if (frameWidth60FPS < 1.f)
							{
								frameWidth60FPS = 1.f;
							}
						}
						else
						{
							if (frameWidth60FPS < 200.f)
							{
								frameWidth60FPS = 200.f;
							}
						}
						callstackLeftOffset = gui.mousePosition.x - oldRelMouseX * frameWidth60FPS;
					}
					if (gui.mouseDown && interaction.downEvent)
					{
						scrollAnchor = gui.mousePosition.x - callstackLeftOffset;
						scrolling = true;
					}
					if (!gui.mouseDown)
					{
						scrolling = false;
					}
					else if (scrolling)
					{
						callstackLeftOffset = gui.mousePosition.x - scrollAnchor;
					}


					if (frame->globalFrameID == -1)
					{
						initializationFrameWidth60FPS = frameWidth60FPS;
						initializationCallstackLeftOffset = callstackLeftOffset;
					}
					else
					{
						normalFrameWidth60FPS = frameWidth60FPS;
						normalCallstackLeftOffset = callstackLeftOffset;
					}
				}
				else
				{
					uint32 row = 0;
					for (auto& it : selectedFrameAccumulatedTimings)
					{
						gui.textAtF(150, topOffset + row * gui.textHeight, 0xFFFFFFFF, "%s: %f ms", it.first, it.second.totalDuration * 1000.f);
						++row;
					}
				}
			}
		}
	}
}

void processAndDisplayProfileEvents(debug_gui& gui)
{
	PROFILE_FUNCTION();

	// Do not remove this line!
	static bool performanceFrequencyQueried = QueryPerformanceFrequency((LARGE_INTEGER*)&performanceFrequency);

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
