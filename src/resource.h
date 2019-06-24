#pragma once

#include <dx/d3dx12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <DirectXTex/DirectXTex/DirectXTex.h>

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

enum texture_usage
{
	texture_usage_albedo,
	texture_usage_normal,
	texture_usage_metallic,
	texture_usage_roughness,
	texture_usage_height,
	texture_usage_render_target, 
};

struct dx_texture
{
	texture_usage usage;
};
