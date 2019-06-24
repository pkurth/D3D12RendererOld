#pragma once

#include <dx/d3dx12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

struct dx_vertex_buffer
{
	ComPtr<ID3D12Resource> buffer;
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_index_buffer
{
	ComPtr<ID3D12Resource> buffer;
	D3D12_INDEX_BUFFER_VIEW view;
};

