#pragma once

#include "common.h"
#include "command_queue.h"
#include "buffer.h"
#include "math.h"

#include <dx/d3dx12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

struct scene_data
{
	dx_vertex_buffer vertexBuffer;
	dx_index_buffer indexBuffer;

	ComPtr<ID3D12RootSignature> rootSignature;
};

class dx_game
{

public:
	void initialize(ComPtr<ID3D12Device2> device, dx_command_queue& copyCommandQueue, uint32 width, uint32 height);
	void resize(uint32 width, uint32 height);

	void update(float dt);
	void render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtv);

private:
	void resizeDepthBuffer(uint32 width, uint32 height);



	bool contentLoaded = false;
	ComPtr<ID3D12Device2> device;

	ComPtr<ID3D12Resource> depthBuffer;
	ComPtr<ID3D12DescriptorHeap> dsvHeap;

	ComPtr<ID3D12PipelineState> pipelineState;

	scene_data scene;

	uint32 width;
	uint32 height;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	mat4 modelMatrix;
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

