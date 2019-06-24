// Most code in this file is from this awesome tutorial: https://www.3dgep.com/learning-directx12-1/


#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

// directx
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <dx/d3dx12.h>

#include <iostream>
#include <chrono>

#include "common.h"
#include "error.h"

static const uint32 numFrames = 3;
static bool useWarp = false;

static uint32 clientWidth = 1280;
static uint32 clientHeight = 720;

static bool initialized = false;

static ComPtr<ID3D12Device2> device;
static ComPtr<ID3D12CommandQueue> commandQueue;
static ComPtr<IDXGISwapChain4> swapChain;
static ComPtr<ID3D12Resource> backBuffers[numFrames];
static ComPtr<ID3D12GraphicsCommandList> commandList;
static ComPtr<ID3D12CommandAllocator> commandAllocators[numFrames];
static ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
static UINT rtvDescriptorSize;
static UINT currentBackBufferIndex;

static ComPtr<ID3D12Fence> fence;
static uint64 fenceValue = 0;
static uint64 frameFenceValues[numFrames] = {};
static HANDLE fenceEvent;

static bool vSync = true;
static bool tearingSupported = false;
static bool fullscreen = false;

HWND windowHandle;
RECT windowRect;

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
	UINT createFactoryFlags = 0;
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
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
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

static ComPtr<ID3D12CommandQueue> createCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	checkResult(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

	return d3d12CommandQueue;
}

static bool checkTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

static ComPtr<IDXGISwapChain4> createSwapChain(HWND hWnd,
	ComPtr<ID3D12CommandQueue> commandQueue,
	uint32 width, uint32 height, uint32 bufferCount)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	checkResult(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	checkResult(dxgiFactory4->CreateSwapChainForHwnd(
		commandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	checkResult(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	checkResult(swapChain1.As(&dxgiSwapChain4));

	return dxgiSwapChain4;
}

static ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(ComPtr<ID3D12Device2> device,
	D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	checkResult(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

static void updateRenderTargetViews(ComPtr<ID3D12Device2> device,
	ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < numFrames; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		checkResult(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		backBuffers[i] = backBuffer;

		rtvHandle.Offset(rtvDescriptorSize);
	}
}

static ComPtr<ID3D12CommandAllocator> createCommandAllocator(ComPtr<ID3D12Device2> device,
	D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	checkResult(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

static ComPtr<ID3D12GraphicsCommandList> createCommandList(ComPtr<ID3D12Device2> device,
	ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	checkResult(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	checkResult(commandList->Close());

	return commandList;
}

static ComPtr<ID3D12Fence> createFence(ComPtr<ID3D12Device2> device)
{
	ComPtr<ID3D12Fence> fence;
	checkResult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

	return fence;
}

static HANDLE createEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

static uint64 signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
	uint64& fenceValue)
{
	uint64 fenceValueForSignal = ++fenceValue;
	checkResult(commandQueue->Signal(fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

static void waitForFenceValue(ComPtr<ID3D12Fence> fence, uint64 fenceValue, HANDLE fenceEvent,
	std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		checkResult(fence->SetEventOnCompletion(fenceValue, fenceEvent));
		WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

static void flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
	uint64& fenceValue, HANDLE fenceEvent)
{
	uint64 fenceValueForSignal = signal(commandQueue, fence, fenceValue);
	waitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

static void update()
{
	static uint64 frameCounter = 0;
	static float elapsedSeconds = 0.f;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	auto t1 = clock.now();
	auto dt = t1 - t0;
	t0 = t1;

	elapsedSeconds += dt.count() * 1e-9f;
	if (elapsedSeconds > 1.f)
	{
		char buffer[500];
		float fps = frameCounter / elapsedSeconds;
		sprintf_s(buffer, sizeof(buffer), "FPS: %f\n", fps);
		OutputDebugString(buffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}

void render()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator = commandAllocators[currentBackBufferIndex];
	ComPtr<ID3D12Resource> backBuffer = backBuffers[currentBackBufferIndex];

	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);

	// Clear backbuffer.
	{
		// Transition backbuffer from "Present" to "Render Target", so we can render to it.
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList->ResourceBarrier(1, &barrier);

		FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			currentBackBufferIndex, rtvDescriptorSize);

		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	}

	// Present to screen.
	{
		// Transition back to "Present".
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &barrier);

		checkResult(commandList->Close());

		ID3D12CommandList* const commandLists[] = {
			commandList.Get()
		};
		commandQueue->ExecuteCommandLists(arraysize(commandLists), commandLists);

		UINT syncInterval = vSync ? 1 : 0;
		UINT presentFlags = tearingSupported && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		checkResult(swapChain->Present(syncInterval, presentFlags));

		frameFenceValues[currentBackBufferIndex] = signal(commandQueue, fence, fenceValue);

		currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

		waitForFenceValue(fence, frameFenceValues[currentBackBufferIndex], fenceEvent);
	}
}

static void resize(uint32 width, uint32 height)
{
	if (clientWidth != width || clientHeight != height)
	{
		clientWidth = max(1u, width);
		clientHeight = max(1u, height);

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		flush(commandQueue, fence, fenceValue, fenceEvent);

		for (uint32 i = 0; i < numFrames; ++i)
		{
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			backBuffers[i].Reset();
			frameFenceValues[i] = frameFenceValues[currentBackBufferIndex];
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		checkResult(swapChain->GetDesc(&swapChainDesc));
		checkResult(swapChain->ResizeBuffers(numFrames, clientWidth, clientHeight,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

		updateRenderTargetViews(device, swapChain, rtvDescriptorHeap);
	}
}

static void setFullscreen(bool fullscreen)
{
	if (::fullscreen != fullscreen)
	{
		::fullscreen = fullscreen;

		if (fullscreen) // Switching to fullscreen.
		{
			GetWindowRect(windowHandle, &windowRect);

			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
			SetWindowLongW(windowHandle, GWL_STYLE, windowStyle);

			HMONITOR hMonitor = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			GetMonitorInfo(hMonitor, &monitorInfo);

			SetWindowPos(windowHandle, HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(windowHandle, SW_MAXIMIZE);
		}
		else
		{
			SetWindowLong(windowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			SetWindowPos(windowHandle, HWND_NOTOPMOST,
				windowRect.left,
				windowRect.top,
				windowRect.right - windowRect.left,
				windowRect.bottom - windowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(windowHandle, SW_NORMAL);
		}
	}
}

static LRESULT CALLBACK windowCallback(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (initialized)
	{
		switch (msg)
		{
			case WM_PAINT:
			{
				update();
				render();
			} break;

			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
			{
				bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

				switch (wParam)
				{
				case 'V':
					vSync = !vSync;
					break;
				case VK_ESCAPE:
					PostQuitMessage(0);
					break;
				case VK_RETURN:
					if (alt)
					{
				case VK_F11:
					setFullscreen(!fullscreen);
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
				GetClientRect(windowHandle, &clientRect);

				int width = clientRect.right - clientRect.left;
				int height = clientRect.bottom - clientRect.top;

				resize(width, height);
			} break;

			case WM_DESTROY:
				PostQuitMessage(0);
				break;

			default:
				return DefWindowProcW(hwnd, msg, wParam, lParam);
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

	tearingSupported = checkTearingSupport();

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

	windowRect = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	windowHandle = CreateWindowEx(0, windowClass.lpszClassName, TEXT("DX12"), WS_OVERLAPPEDWINDOW,
#if 1
		CW_USEDEFAULT, CW_USEDEFAULT,
#else
		0, 0
#endif
		windowWidth, windowHeight,
		0, 0, 0, 0);

	GetWindowRect(windowHandle, &windowRect);



	ComPtr<IDXGIAdapter4> dxgiAdapter4 = getAdapter(useWarp);

	device = createDevice(dxgiAdapter4);

	commandQueue = createCommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);

	swapChain = createSwapChain(windowHandle, commandQueue, clientWidth, clientHeight, numFrames);

	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	rtvDescriptorHeap = createDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numFrames);
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	updateRenderTargetViews(device, swapChain, rtvDescriptorHeap);

	for (uint32 i = 0; i < numFrames; ++i)
	{
		commandAllocators[i] = createCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	commandList = createCommandList(device, commandAllocators[currentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

	fence = createFence(device);
	fenceEvent = createEventHandle();



	initialized = true;

	ShowWindow(windowHandle, SW_SHOW);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	flush(commandQueue, fence, fenceValue, fenceEvent);

	CloseHandle(fenceEvent);

	return 0;
}
