#pragma once

#include "resource.h"

struct dx_vertex_buffer
{
	ComPtr<ID3D12Resource> resource;
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_index_buffer
{
	ComPtr<ID3D12Resource> resource;
	D3D12_INDEX_BUFFER_VIEW view;
	uint32 numIndices;
};

struct dx_mesh
{
	dx_vertex_buffer vertexBuffer;
	dx_index_buffer indexBuffer;
};

