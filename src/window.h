#pragma once


#include "common.h"



class dx_window
{
public:
	void initialize(const TCHAR* windowClassName, ComPtr<ID3D12Device2> device,
		uint32 clientWidth, uint32 clientHeight, color_depth colorDepth = color_depth_8, bool exclusiveFullscreen = false);

public:
	static const uint32 numFrames = 3;

	void resize(uint32 width, uint32 height);
	void toggleFullscreen();
	void onMove();
	void onDisplayChange();

	uint32 present();

	inline uint32 getCurrentBackBufferIndex() const { return currentBackBufferIndex; }
	inline CD3DX12_CPU_DESCRIPTOR_HANDLE getCurrentRenderTargetView()
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			currentBackBufferIndex, rtvDescriptorSize);
		return rtv;
	}
	inline ComPtr<ID3D12Resource> getCurrentBackBuffer()
	{
		return backBuffers[currentBackBufferIndex];
	}

private:
	void updateRenderTargetViews();

	ComPtr<IDXGIFactory4> factory;
	ComPtr<ID3D12Device2> device;
	ComPtr<IDXGISwapChain4> swapChain;
	ComPtr<ID3D12Resource> backBuffers[numFrames];
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	uint32 rtvDescriptorSize;
	uint32 currentBackBufferIndex;

	bool vSync = false;
	bool tearingSupported = false;
	bool fullscreen = false;
	bool initialized = false;
	bool hdrSupport = false;
	bool exclusiveFullscreen;

	HWND windowHandle;
	RECT windowRectBeforeFullscreen;

	uint32 clientWidth;
	uint32 clientHeight;

	color_depth colorDepth;

	friend LRESULT CALLBACK windowCallback(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam);
};
