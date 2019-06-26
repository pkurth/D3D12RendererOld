#include "window.h"
#include "error.h"
#include "commands.h"
#include "resource_state_tracker.h"

#include <iostream>


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
	uint32 width, uint32 height, uint32 bufferCount, bool tearingSupported)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	uint32 createFactoryFlags = 0;
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

void dx_window::initialize(const TCHAR* windowClassName, ComPtr<ID3D12Device2> device,
	uint32 clientWidth, uint32 clientHeight, const dx_command_queue& commandQueue)
{
	this->device = device;
	this->clientWidth = clientWidth;
	this->clientHeight = clientHeight;
	tearingSupported = checkTearingSupport();

	
	windowRect = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	windowHandle = CreateWindowEx(0, windowClassName, TEXT("DX12"), WS_OVERLAPPEDWINDOW,
#if 1
		CW_USEDEFAULT, CW_USEDEFAULT,
#else
		0, 0
#endif
		windowWidth, windowHeight,
		0, 0, 0, 0);

	SetWindowLongPtr(windowHandle, GWLP_USERDATA, (LONG_PTR)this);

	GetWindowRect(windowHandle, &windowRect);

	swapChain = createSwapChain(windowHandle, commandQueue.getD3D12CommandQueue(), clientWidth, clientHeight, numFrames, tearingSupported);
	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	rtvDescriptorHeap = createDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numFrames);
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	updateRenderTargetViews();

	initialized = true;

	ShowWindow(windowHandle, SW_SHOW);
}

void dx_window::updateRenderTargetViews()
{
	uint32 rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < numFrames; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		checkResult(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		dx_resource_state_tracker::addGlobalResourceState(backBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, 1);

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		backBuffers[i] = backBuffer;

		rtvHandle.Offset(rtvDescriptorSize);
	}
}

void dx_window::resize(uint32 width, uint32 height)
{
	if (clientWidth != width || clientHeight != height)
	{
		clientWidth = max(1u, width);
		clientHeight = max(1u, height);

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		flushApplication();

		for (uint32 i = 0; i < numFrames; ++i)
		{
			dx_resource_state_tracker::removeGlobalResourceState(backBuffers[i].Get());
			backBuffers[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		checkResult(swapChain->GetDesc(&swapChainDesc));
		checkResult(swapChain->ResizeBuffers(numFrames, clientWidth, clientHeight,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

		updateRenderTargetViews();
	}
}

void dx_window::toggleFullscreen()
{
	fullscreen = !fullscreen;

	if (fullscreen) // Switching to fullscreen.
	{
		GetWindowRect(windowHandle, &windowRect);

		uint32 windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
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

uint32 dx_window::present()
{
	uint32 syncInterval = vSync ? 1 : 0;
	uint32 presentFlags = tearingSupported && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	checkResult(swapChain->Present(syncInterval, presentFlags));

	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
	return currentBackBufferIndex;
}
