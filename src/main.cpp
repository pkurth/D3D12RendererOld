#include "pch.h"

// Most code in this file is from this awesome tutorial: https://www.3dgep.com/learning-directx12-1/

#include "window.h"
#include "common.h"
#include "error.h"
#include "command_queue.h"
#include "game.h"
#include "descriptor_allocator.h"
#include "graphics.h"
#include "platform.h"
#include "profiling.h"

#include <windowsx.h>


static bool exclusiveFullscreen = false;
static color_depth colorDepth = color_depth_8;
static bool useWarp = false;

static dx_window window;
static dx_game game;

static ComPtr<ID3D12Device2> device;

static std::vector<std::function<bool(keyboard_event event)>> keyDownCallbacks;
static std::vector<std::function<bool(keyboard_event event)>> keyUpCallbacks;
static std::vector<std::function<bool(character_event event)>> characterCallbacks;

static std::vector<std::function<bool(mouse_button_event event)>> mouseButtonDownCallbacks;
static std::vector<std::function<bool(mouse_button_event event)>> mouseButtonUpCallbacks;
static std::vector<std::function<bool(mouse_move_event event)>> mouseMoveCallbacks;
static std::vector<std::function<bool(mouse_scroll_event event)>> mouseScrollCallbacks;


static bool initialized = false;

static HWND currentHoverHWND = 0;

static void enableDebugLayer()
{
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
	checkResult(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

static ComPtr<IDXGIAdapter4> getAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	uint32 createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	checkResult(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		checkResult(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		checkResult(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		size_t maxDedicatedVideoMemory = 0;
		for (uint32 i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				checkResult(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

static ComPtr<ID3D12Device2> createDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	checkResult(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(d3d12Device2.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID denyIDs[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER newFilter = {};
		//NewFilter.DenyList.NumCategories = arraysize(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		newFilter.DenyList.NumSeverities = arraysize(severities);
		newFilter.DenyList.pSeverityList = severities;
		newFilter.DenyList.NumIDs = arraysize(denyIDs);
		newFilter.DenyList.pIDList = denyIDs;

		checkResult(infoQueue->PushStorageFilter(&newFilter));
	}
#endif

	return d3d12Device2;
}

void flushApplication()
{
	dx_command_queue::renderCommandQueue.flush();
	dx_command_queue::computeCommandQueue.flush();
	dx_command_queue::copyCommandQueue.flush();
}

void registerKeyDownCallback(std::function<bool(keyboard_event)> func)
{
	keyDownCallbacks.push_back(func);
}

void registerKeyUpCallback(std::function<bool(keyboard_event)> func)
{
	keyUpCallbacks.push_back(func);
}

void registerCharacterCallback(std::function<bool(character_event)> func)
{
	characterCallbacks.push_back(func);
}

void registerMouseButtonDownCallback(std::function<bool(mouse_button_event)> func)
{
	mouseButtonDownCallbacks.push_back(func);
}

void registerMouseButtonUpCallback(std::function<bool(mouse_button_event)> func)
{
	mouseButtonUpCallbacks.push_back(func);
}

void registerMouseMoveCallback(std::function<bool(mouse_move_event)> func)
{
	mouseMoveCallbacks.push_back(func);
}

void registerMouseScrollCallback(std::function<bool(mouse_scroll_event)> func)
{
	mouseScrollCallbacks.push_back(func);
}

template <typename T>
static void callCallback(std::vector<std::function<bool(T)>>& callbacks, const T& event)
{
	for (auto& func : callbacks)
	{
		if (func(event))
		{
			break; // Event handled.
		}
	}
}

static keyboard_key mapVKCodeToKey(uint32 vkCode)
{
	if (vkCode >= '0' && vkCode <= '9')
	{
		return (keyboard_key)(vkCode - 48);
	}
	else if (vkCode >= 'A' && vkCode <= 'Z')
	{
		return (keyboard_key)(vkCode - 55);
	}
	else
	{
		switch (vkCode)
		{
		case VK_SPACE: return key_space;
		case VK_TAB: return key_tab;
		case VK_RETURN: return key_enter;
		case VK_SHIFT: return key_shift;
		case VK_CONTROL: return key_ctrl;
		case VK_ESCAPE: return key_esc;
		case VK_UP: return key_up;
		case VK_DOWN: return key_down;
		case VK_LEFT: return key_left;
		case VK_RIGHT: return key_right;
		case VK_MENU: return key_alt;
		case VK_BACK: return key_backspace;
		case VK_DELETE: return key_delete;
		}
	}
	return key_unknown;
}

LRESULT CALLBACK windowCallback(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	dx_window* window = (dx_window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (window && window->initialized)
	{
		static bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
		static bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		static bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		static bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		static bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		static bool middle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

		static float lastRelMouseX = 0.f;
		static float lastRelMouseY = 0.f;

		uint32 mouseX = GET_X_LPARAM(lParam); // Relative to client area. This is only valid, if the message is mouse related.
		uint32 mouseY = GET_Y_LPARAM(lParam);
		float relMouseX = (float)mouseX / (window->clientWidth - 1);
		float relMouseY = (float)mouseY / (window->clientHeight - 1);

		switch (msg)
		{
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			bool wasDown = ((lParam & (1 << 30)) != 0);

			if (!wasDown)
			{
				switch (wParam)
				{
				case 'V':
					window->vSync = !window->vSync;
					break;
				case VK_ESCAPE:
					PostQuitMessage(0);
					break;

				case VK_RETURN:
					if (alt)
					{
						window->toggleFullscreen();
					}
					break;

				default:
				{
					keyboard_event event = { mapVKCodeToKey((uint32)wParam), shift, ctrl, alt };
					if (event.key != key_unknown)
					{
						callCallback(keyDownCallbacks, event);
					}
					if (event.key == key_shift)
					{
						shift = true;
					}
					else if (event.key == key_ctrl)
					{
						ctrl = true;
					}
					else if (event.key == key_alt)
					{
						alt = true;
					}
				} break;
				}
			}
		} break;

		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			keyboard_event event = { mapVKCodeToKey((uint32)wParam), shift, ctrl, alt };
			if (event.key != key_unknown)
			{
				callCallback(keyUpCallbacks, event);
			}
			if (event.key == key_shift)
			{
				shift = false;
			}
			else if (event.key == key_ctrl)
			{
				ctrl = false;
			}
			else if (event.key == key_alt)
			{
				alt = false;
			}
		} break;

		case WM_UNICHAR:
		{
			character_event event = { (uint32)wParam };
			callCallback(characterCallbacks, event);
		} break;

		case WM_LBUTTONDOWN:
		{
			mouse_button_event event = { mouse_left, mouseX, mouseY, relMouseX, relMouseY, shift, ctrl, alt };
			callCallback(mouseButtonDownCallbacks, event);
			left = true;
		} break;

		case WM_LBUTTONUP:
		{
			mouse_button_event event = { mouse_left, mouseX, mouseY, relMouseX, relMouseY, shift, ctrl, alt };
			callCallback(mouseButtonUpCallbacks, event);
			left = false;
		} break;

		case WM_RBUTTONDOWN:
		{
			mouse_button_event event = { mouse_right, mouseX, mouseY, relMouseX, relMouseY, shift, ctrl, alt };
			callCallback(mouseButtonDownCallbacks, event);
			right = true;
		} break;

		case WM_RBUTTONUP:
		{
			mouse_button_event event = { mouse_right, mouseX, mouseY, relMouseX, relMouseY, shift, ctrl, alt };
			callCallback(mouseButtonUpCallbacks, event);
			right = false;
		} break;

		case WM_MBUTTONDOWN:
		{
			mouse_button_event event = { mouse_middle, mouseX, mouseY, relMouseX, relMouseY, shift, ctrl, alt };
			callCallback(mouseButtonDownCallbacks, event);
			middle = true;
		} break;

		case WM_MBUTTONUP:
		{
			mouse_button_event event = { mouse_middle, mouseX, mouseY, relMouseX, relMouseY, shift, ctrl, alt };
			callCallback(mouseButtonUpCallbacks, event);
			middle = false;
		} break;

		case WM_MOUSEWHEEL:
		{
			mouse_scroll_event event = { GET_WHEEL_DELTA_WPARAM(wParam) / 120.f, mouseX, mouseY, relMouseX, relMouseY, left, right, middle, shift, ctrl, alt };
			callCallback(mouseScrollCallbacks, event);
		} break;

		case WM_MOUSEMOVE:
		{
			float relDX = relMouseX - lastRelMouseX;
			float relDY = relMouseY - lastRelMouseY;
			mouse_move_event event = { mouseX, mouseY, relMouseX, relMouseY, relDX, relDY, left, right, middle, shift, ctrl, alt };
			
			if (currentHoverHWND != hwnd)
			{
				// Mouse has entered window.
				TRACKMOUSEEVENT mouseEvent = { sizeof(TRACKMOUSEEVENT) };
				mouseEvent.dwFlags = TME_LEAVE;
				mouseEvent.hwndTrack = hwnd;
				TrackMouseEvent(&mouseEvent);

				currentHoverHWND = hwnd;
			}

			callCallback(mouseMoveCallbacks, event);

			lastRelMouseX = relMouseX;
			lastRelMouseY = relMouseY;

		} break;

		case WM_NCMOUSELEAVE:
		case WM_MOUSELEAVE:
		{
			// Mouse has left window.
			if (currentHoverHWND == hwnd)
			{
				currentHoverHWND = 0;
			}
		} break;

		case WM_ACTIVATEAPP:
		{
			if (wParam == TRUE) // App is reactivated. Get all the key states again.
			{
				alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
				shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
				ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
				left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
				right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
				middle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
			}
		} break;


		// The default window procedure will play a system notification sound 
		// when pressing the Alt+Enter keyboard combination if this message is 
		// not handled.
		case WM_SYSCHAR:
			break;

		case WM_SIZE:
		{
			RECT clientRect = {};
			GetClientRect(hwnd, &clientRect);

			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;

			window->resize(width, height);

			if (initialized)
			{
				game.resize(width, height);
			}
		} break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		return 0;
	}
	else
	{
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

int main()
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	enableDebugLayer();

	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &windowCallback;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = NULL;
	windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = TEXT("WINDOWCLASS");
	windowClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&windowClass))
	{
		std::cerr << "Failed to create window class." << std::endl;
		return 1;
	}



	ComPtr<IDXGIAdapter4> dxgiAdapter4 = getAdapter(useWarp);
	device = createDevice(dxgiAdapter4);

	dx_descriptor_allocator::initialize(device);
	dx_command_queue::renderCommandQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	dx_command_queue::computeCommandQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	dx_command_queue::copyCommandQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COPY);

	initializeCommonGraphicsItems();

	uint32 initialWidth = 1280;
	uint32 initialHeight = 720;

	window.initialize(windowClass.lpszClassName, device, initialWidth, initialHeight, colorDepth, exclusiveFullscreen);
	game.initialize(device, initialWidth, initialHeight, colorDepth);


	uint64 fenceValues[dx_window::numFrames] = {};
	uint64 frameValues[dx_window::numFrames] = {};

	uint64 frameID = 0;


	initialized = true;


	// Main loop.

	dx_command_queue& renderCommandQueue = dx_command_queue::renderCommandQueue;

	std::chrono::high_resolution_clock clock;
	std::chrono::time_point now = clock.now();
	std::chrono::time_point lastBeforeUpdate = now;

	uint32 currentBackBufferIndex = window.getCurrentBackBufferIndex();;

	bool running = true;
	while (running)
	{
		PROFILE_FRAME(frameID);

		// Input and message processing.
		MSG msg = {};
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}


		// Update.
		now = clock.now();
		float dt = (now - lastBeforeUpdate).count() * 1e-9f;
		lastBeforeUpdate = now;

		game.update(frameID, dt);



		// Render.

		// Make sure, that the backbuffer to use in this frame is actually ready for use again.
		renderCommandQueue.waitForFenceValue(fenceValues[currentBackBufferIndex]);
		dx_descriptor_allocator::releaseStaleDescriptors(frameValues[currentBackBufferIndex]);


		ComPtr<ID3D12Resource> backBuffer = window.getCurrentBackBuffer();
		dx_command_list* commandList = renderCommandQueue.getAvailableCommandList();

		// Transition backbuffer from "Present" to "Render Target", so we can render to it.
		commandList->transitionBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv = window.getCurrentRenderTargetView();

		game.render(commandList, rtv);

		// Transition back to "Present".
		commandList->transitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);

		// Run command list.
		fenceValues[currentBackBufferIndex] = renderCommandQueue.executeCommandList(commandList);



		frameValues[currentBackBufferIndex] = frameID;
		currentBackBufferIndex = window.present();

		++frameID;
	}

	flushApplication();

	return 0;
}
