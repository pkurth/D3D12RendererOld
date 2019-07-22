#pragma once

#include "common.h"
#include "command_queue.h"
#include "resource.h"
#include "root_signature.h"
#include "math.h"
#include "camera.h"
#include "render_target.h"


struct scene_data
{
	std::vector<dx_texture> textures;
	std::vector<dx_mesh> meshes;
	
	dx_mesh skyMesh;
	dx_texture equirectangular;
	dx_texture cubemap;
	dx_texture irradiance;
	dx_texture prefilteredEnvironment;
	dx_texture brdf;

	dx_mesh quad;
};

class dx_game
{

public:
	void initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height, color_depth colorDepth = color_depth_8);
	void resize(uint32 width, uint32 height);

	void update(float dt);
	void render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtv);

private:
	void resizeDepthBuffer(uint32 width, uint32 height);



	bool contentLoaded = false;
	ComPtr<ID3D12Device2> device;

	ComPtr<ID3D12Resource> depthBuffer;
	ComPtr<ID3D12DescriptorHeap> dsvHeap;

	ComPtr<ID3D12PipelineState> geometryPipelineState;
	dx_root_signature geometryRootSignature;

	ComPtr<ID3D12PipelineState> skyPipelineState;
	dx_root_signature skyRootSignature;

	scene_data scene;

	uint32 width;
	uint32 height;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	mat4 modelMatrix;
	
	render_camera camera;

	dx_render_target gBufferRT;
	dx_render_target lightingRT;
};

