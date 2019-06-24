#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include "common.h"
#include "command_queue.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

void flushApplication();

class dx_window
{
public:
	void initialize(const TCHAR* windowClassName, ComPtr<ID3D12Device2> device,
		uint32 clientWidth, uint32 clientHeight, const dx_command_queue& commandQueue);

public:
	static const uint32 numFrames = 3;

	void resize(uint32 width, uint32 height);
	void toggleFullscreen();

private:
	void updateRenderTargetViews();

	ComPtr<ID3D12Device2> device;
	ComPtr<IDXGISwapChain4> swapChain;
	ComPtr<ID3D12Resource> backBuffers[numFrames];
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	uint32 rtvDescriptorSize;
	uint32 currentBackBufferIndex;

	bool vSync = true;
	bool tearingSupported = false;
	bool fullscreen = false;
	bool initialized = false;

	HWND windowHandle;
	RECT windowRect;

	uint32 clientWidth;
	uint32 clientHeight;

	friend LRESULT CALLBACK windowCallback(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam);
	friend void render(dx_window* window);
};
