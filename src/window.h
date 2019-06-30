#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "common.h"
#include "command_queue.h"

#include <dx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

class dx_window
{
public:
	void initialize(const TCHAR* windowClassName, ComPtr<ID3D12Device2> device,
		uint32 clientWidth, uint32 clientHeight);

public:
	static const uint32 numFrames = 3;

	void resize(uint32 width, uint32 height);
	void toggleFullscreen();

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

	HWND windowHandle;
	RECT windowRect;

	uint32 clientWidth;
	uint32 clientHeight;

	friend LRESULT CALLBACK windowCallback(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam);
};
