#pragma once

#include "common.h"
#include "model.h"

template <typename index_t> inline DXGI_FORMAT getFormat() { static_assert(false, "Unknown index format."); return DXGI_FORMAT_UNKNOWN; }
template <>					inline DXGI_FORMAT getFormat<uint16>() { return DXGI_FORMAT_R16_UINT; }
template <>					inline DXGI_FORMAT getFormat<uint32>() { return DXGI_FORMAT_R32_UINT; }

class dx_command_list;

struct dx_buffer
{
	ComPtr<ID3D12Resource> resource;
	ComPtr<ID3D12Device2> device;

	void initialize(ComPtr<ID3D12Device2> device, uint32 size, const void* data = nullptr, dx_command_list* commandList = nullptr, 
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	template <typename T> void initialize(ComPtr<ID3D12Device2> device, const T* data, uint32 count, dx_command_list* commandList = nullptr,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
	{ 
		initialize(device, sizeof(T) * count, data, commandList, flags); 
	}

	void copyBackToCPU(void* buffer, uint32 size);
};

struct dx_vertex_buffer : dx_buffer
{
	D3D12_VERTEX_BUFFER_VIEW view;

	template <typename vertex_t> void initialize(ComPtr<ID3D12Device2> device, vertex_t* vertices, uint32 count, dx_command_list* commandList = nullptr);
};

struct dx_index_buffer : dx_buffer
{
	D3D12_INDEX_BUFFER_VIEW view;
	uint32 numIndices;

	template <typename index_t> void initialize(ComPtr<ID3D12Device2> device, index_t* indices, uint32 count, dx_command_list* commandList = nullptr);
};

struct dx_structured_buffer : dx_buffer
{
	D3D12_CPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE uav;

	uint32 count;
	uint32 elementSize;

	void initialize(ComPtr<ID3D12Device2> device, uint32 count, uint32 elementSize, const void* data = nullptr, dx_command_list* commandList = nullptr);
	template <typename T> void initialize(ComPtr<ID3D12Device2> device, const T* data, uint32 count, dx_command_list* commandList = nullptr)
	{
		initialize(device, count, (uint32)sizeof(T), data, commandList);
	}

	void createShaderResourceView(ComPtr<ID3D12Device2> device, D3D12_CPU_DESCRIPTOR_HANDLE srv);
};

struct dx_mesh
{
	dx_vertex_buffer vertexBuffer;
	dx_index_buffer indexBuffer;

	template <typename vertex_t> void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const cpu_triangle_mesh<vertex_t>& cpuMesh);
};


template<typename vertex_t>
inline void dx_vertex_buffer::initialize(ComPtr<ID3D12Device2> device, vertex_t* vertices, uint32 count, dx_command_list* commandList)
{
	dx_buffer::initialize(device, vertices, count, commandList);

	view.BufferLocation = resource->GetGPUVirtualAddress();
	view.SizeInBytes = count * sizeof(vertex_t);
	view.StrideInBytes = sizeof(vertex_t);
}

template<typename index_t>
inline void dx_index_buffer::initialize(ComPtr<ID3D12Device2> device, index_t* indices, uint32 count, dx_command_list* commandList)
{
	dx_buffer::initialize(device, indices, count, commandList);

	view.BufferLocation = resource->GetGPUVirtualAddress();
	view.Format = getFormat<index_t>();
	view.SizeInBytes = count * sizeof(index_t);
	numIndices = count;
}

template<typename vertex_t>
inline void dx_mesh::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const cpu_triangle_mesh<vertex_t>& cpuMesh)
{
	vertexBuffer.initialize(device, cpuMesh.vertices.data(), (uint32)cpuMesh.vertices.size(), commandList);
	indexBuffer.initialize(device, (decltype(cpuMesh.triangles.data()->a)*)cpuMesh.triangles.data(), (uint32)cpuMesh.triangles.size() * 3, commandList);
}
