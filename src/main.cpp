#include "pch.h"

// Most code in this file is from this awesome tutorial: https://www.3dgep.com/learning-directx12-1/

#include "window.h"
#include "common.h"
#include "error.h"
#include "command_queue.h"
#include "game.h"
#include "descriptor_allocator.h"
#include "graphics.h"


static bool exclusiveFullscreen = false;
static color_depth colorDepth = color_depth_8;
static bool useWarp = false;

static dx_window window;
static dx_game game;

static ComPtr<ID3D12Device2> device;

static uint64 fenceValues[dx_window::numFrames] = {};
static uint64 frameValues[dx_window::numFrames] = {};

static uint64 frameCount = 0;

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

static void update()
{
	static uint64 frameCounter = 0;
	static float elapsedSeconds = 0.f;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	auto t1 = clock.now();
	auto deltaTime = t1 - t0;
	t0 = t1;

	float dt = deltaTime.count() * 1e-9f;
	elapsedSeconds += dt;
	if (elapsedSeconds > 1.f)
	{
		char buffer[500];
		float fps = frameCounter / elapsedSeconds;
		sprintf_s(buffer, sizeof(buffer), "FPS: %f\n", fps);
		std::cout << buffer << std::endl;

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}

	game.update(dt);
}

void render(dx_window* window)
{
	ComPtr<ID3D12Resource> backBuffer = window->getCurrentBackBuffer();

	dx_command_queue& renderCommandQueue = dx_command_queue::renderCommandQueue;
	dx_command_list* commandList = renderCommandQueue.getAvailableCommandList();

	// Transition backbuffer from "Present" to "Render Target", so we can render to it.
	commandList->transitionBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

	uint32 currentBackBufferIndex = window->getCurrentBackBufferIndex();
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv = window->getCurrentRenderTargetView();

	game.render(commandList, rtv);

	// Transition back to "Present".
	commandList->transitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);

	// Run command list and wait for next one to become free.
	fenceValues[currentBackBufferIndex] = renderCommandQueue.executeCommandList(commandList);
	frameValues[currentBackBufferIndex] = frameCount;
	uint32 newCurrentBackbufferIndex = window->present();

	// Make sure, that command queue is finished.
	renderCommandQueue.waitForFenceValue(fenceValues[newCurrentBackbufferIndex]);

	dx_descriptor_allocator::releaseStaleDescriptors(frameValues[newCurrentBackbufferIndex]);
}

void flushApplication()
{
	dx_command_queue::renderCommandQueue.flush();
	dx_command_queue::computeCommandQueue.flush();
	dx_command_queue::copyCommandQueue.flush();
}

LRESULT CALLBACK windowCallback(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	dx_window* window = (dx_window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (window && window->initialized)
	{
		switch (msg)
		{
		case WM_PAINT:
		{
			++frameCount;

			update();
			render(window);
		} break;

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

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
						if (exclusiveFullscreen)
						{
							return DefWindowProc(hwnd, msg, wParam, lParam);
						}
						else
						{
							case VK_F11:
								window->toggleFullscreen();
						}
					}
					break;
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
			game.resize(width, height);
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
		std::cerr << "failed to create window class" << std::endl;
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


	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	flushApplication();

	return 0;
}
