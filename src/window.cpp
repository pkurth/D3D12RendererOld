#include "pch.h"
#include "window.h"
#include "error.h"
#include "commands.h"
#include "resource_state_tracker.h"
#include "command_queue.h"


static ComPtr<IDXGIFactory4> createFactory()
{
	ComPtr<IDXGIFactory4> dxgiFactory4;
	uint32 createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	checkResult(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	return dxgiFactory4;
}

static bool checkTearingSupport(ComPtr<IDXGIFactory4> factory4)
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
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

	return allowTearing == TRUE;
}

static ComPtr<IDXGISwapChain4> createSwapChain(HWND hWnd,
	ComPtr<IDXGIFactory4> factory4, ComPtr<ID3D12CommandQueue> commandQueue,
	uint32 width, uint32 height, uint32 bufferCount, bool tearingSupported, color_depth colorDepth, bool exclusiveFullscreen)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	if (colorDepth == color_depth_8)
	{
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else
	{
		assert(colorDepth == color_depth_10);
		swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	}
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
	checkResult(factory4->CreateSwapChainForHwnd(
		commandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	UINT flags = 0;
	if (!exclusiveFullscreen)
	{
		// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
		// will be handled manually.
		flags = DXGI_MWA_NO_ALT_ENTER;
	}
	checkResult(factory4->MakeWindowAssociation(hWnd, flags));

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

static int32 computeIntersectionArea(int32 ax1, int32 ay1, int32 ax2, int32 ay2, int32 bx1, int32 by1, int32 bx2, int32 by2)
{
	return max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1));
}

static bool checkForHDRSupport(ComPtr<IDXGIFactory4>& factory4, RECT windowRect, color_depth colorDepth)
{
	if (colorDepth == color_depth_8)
	{
		return false;
	}

	if (!factory4->IsCurrent())
	{
		checkResult(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory4)));
	}

	ComPtr<IDXGIAdapter1> dxgiAdapter;
	checkResult(factory4->EnumAdapters1(0, &dxgiAdapter));

	uint32 i = 0;
	ComPtr<IDXGIOutput> currentOutput;
	ComPtr<IDXGIOutput> bestOutput;
	int32 bestIntersectArea = -1;

	while (dxgiAdapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
	{
		// Get the retangle bounds of the app window.
		int ax1 = windowRect.left;
		int ay1 = windowRect.top;
		int ax2 = windowRect.right;
		int ay2 = windowRect.bottom;

		// Get the rectangle bounds of current output.
		DXGI_OUTPUT_DESC desc;
		checkResult(currentOutput->GetDesc(&desc));
		RECT r = desc.DesktopCoordinates;
		int bx1 = r.left;
		int by1 = r.top;
		int bx2 = r.right;
		int by2 = r.bottom;

		// Compute the intersection.
		int32 intersectArea = computeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
		if (intersectArea > bestIntersectArea)
		{
			bestOutput = currentOutput;
			bestIntersectArea = intersectArea;
		}

		++i;
	}

	ComPtr<IDXGIOutput6> output6;
	checkResult(bestOutput.As(&output6));

	DXGI_OUTPUT_DESC1 desc1;
	checkResult(output6->GetDesc1(&desc1));

	return desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
}

static void setSwapChainColorSpace(ComPtr<IDXGISwapChain4> swapChain, color_depth colorDepth, bool hdrSupport)
{
	// Rec2020 is the standard for UHD displays. The tonemap shader needs to apply the ST2084 curve before display.
	// Rec709 is the same as sRGB, just without the gamma curve. The tonemap shader needs to apply the gamma curve before display.
	DXGI_COLOR_SPACE_TYPE colorSpace = (hdrSupport && colorDepth == color_depth_10) ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	UINT colorSpaceSupport = 0;
	if (SUCCEEDED(swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) &&
		((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
	{
		checkResult(swapChain->SetColorSpace1(colorSpace));
	}

	if (!hdrSupport)
	{
		checkResult(swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
		return;
	}

	struct display_chromaticities
	{
		float redX;
		float redY;
		float greenX;
		float greenY;
		float blueX;
		float blueY;
		float whiteX;
		float whiteY;
	};

	static const display_chromaticities chroma =
	{
		0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f // Display Gamut Rec2020
	};

	float maxOutputNits = 1000.f;
	float minOutputNits = 0.001f;
	float maxCLL = 2000.f;
	float maxFALL = 500.f;

	DXGI_HDR_METADATA_HDR10 hdr10MetaData = {};
	hdr10MetaData.RedPrimary[0] = (uint16)(chroma.redX * 50000.f);
	hdr10MetaData.RedPrimary[1] = (uint16)(chroma.redY * 50000.f);
	hdr10MetaData.GreenPrimary[0] = (uint16)(chroma.greenX * 50000.f);
	hdr10MetaData.GreenPrimary[1] = (uint16)(chroma.greenY * 50000.f);
	hdr10MetaData.BluePrimary[0] = (uint16)(chroma.blueX * 50000.f);
	hdr10MetaData.BluePrimary[1] = (uint16)(chroma.blueY * 50000.f);
	hdr10MetaData.WhitePoint[0] = (uint16)(chroma.whiteX * 50000.f);
	hdr10MetaData.WhitePoint[1] = (uint16)(chroma.whiteY * 50000.f);
	hdr10MetaData.MaxMasteringLuminance = (uint32)(maxOutputNits * 10000.f);
	hdr10MetaData.MinMasteringLuminance = (uint32)(minOutputNits * 10000.f);
	hdr10MetaData.MaxContentLightLevel = (uint16)(maxCLL);
	hdr10MetaData.MaxFrameAverageLightLevel = (uint16)(maxFALL);

	checkResult(swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10MetaData));
}

void dx_window::initialize(const TCHAR* windowClassName, ComPtr<ID3D12Device2> device,
	uint32 clientWidth, uint32 clientHeight, color_depth colorDepth, bool exclusiveFullscreen)
{
	this->device = device;
	this->clientWidth = clientWidth;
	this->clientHeight = clientHeight;
	this->colorDepth = colorDepth;
	this->exclusiveFullscreen = exclusiveFullscreen;

	factory = createFactory();
	tearingSupported = checkTearingSupport(factory);

	
	RECT windowRect = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
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
	
	const dx_command_queue& commandQueue = dx_command_queue::renderCommandQueue;
	swapChain = createSwapChain(windowHandle, factory, commandQueue.getD3D12CommandQueue(), clientWidth, clientHeight, numFrames, tearingSupported, colorDepth, exclusiveFullscreen);
	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	rtvDescriptorHeap = createDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numFrames);
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	GetWindowRect(windowHandle, &windowRect);
	hdrSupport = checkForHDRSupport(factory, windowRect, colorDepth);
	setSwapChainColorSpace(swapChain, colorDepth, hdrSupport);

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

		RECT windowRect;
		GetWindowRect(windowHandle, &windowRect);
		hdrSupport = checkForHDRSupport(factory, windowRect, colorDepth);
		setSwapChainColorSpace(swapChain, colorDepth, hdrSupport);

		currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

		updateRenderTargetViews();
	}
}

void dx_window::toggleFullscreen()
{
	fullscreen = !fullscreen;

	if (fullscreen) // Switching to fullscreen.
	{
		GetWindowRect(windowHandle, &windowRectBeforeFullscreen);

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
			windowRectBeforeFullscreen.left,
			windowRectBeforeFullscreen.top,
			windowRectBeforeFullscreen.right - windowRectBeforeFullscreen.left,
			windowRectBeforeFullscreen.bottom - windowRectBeforeFullscreen.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(windowHandle, SW_NORMAL);
	}
}

void dx_window::onMove()
{
	RECT windowRect;
	GetWindowRect(windowHandle, &windowRect);
	hdrSupport = checkForHDRSupport(factory, windowRect, colorDepth);
	setSwapChainColorSpace(swapChain, colorDepth, hdrSupport);
}

void dx_window::onDisplayChange()
{
	RECT windowRect;
	GetWindowRect(windowHandle, &windowRect);
	hdrSupport = checkForHDRSupport(factory, windowRect, colorDepth);
	setSwapChainColorSpace(swapChain, colorDepth, hdrSupport);
}

uint32 dx_window::present()
{
	uint32 syncInterval = vSync ? 1 : 0;
	uint32 presentFlags = tearingSupported && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	checkResult(swapChain->Present(syncInterval, presentFlags));

	currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
	return currentBackBufferIndex;
}
