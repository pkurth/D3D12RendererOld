#pragma once

#include "common.h"
#include "command_queue.h"
#include "resource.h"
#include "root_signature.h"
#include "math.h"
#include "camera.h"
#include "render_target.h"
#include "material.h"
#include "debug_gui.h"
#include "platform.h"

#define GEOMETRY_ROOTPARAM_CAMERA	0
#define GEOMETRY_ROOTPARAM_MODEL	1
#define GEOMETRY_ROOTPARAM_TEXTURES	2

#define SKY_ROOTPARAM_VP			0
#define SKY_ROOTPARAM_TEXTURE		1

#define AMBIENT_ROOTPARAM_CAMERA	0
#define AMBIENT_ROOTPARAM_TEXTURES	1

#define PRESENT_ROOTPARAM_CAMERA	0
#define PRESENT_ROOTPARAM_MODE		1
#define PRESENT_ROOTPARAM_TEXTURE	2

#define NUM_RANDOM_OBJECTS (1024)

class dx_game
{

public:
	void initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height, color_depth colorDepth = color_depth_8);
	void resize(uint32 width, uint32 height);

	void update(float dt);
	void render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV);

	bool keyboardCallback(key_input_event event);
	bool mouseCallback(mouse_input_event event);

private:

	bool contentLoaded = false;
	ComPtr<ID3D12Device2> device;

	ComPtr<ID3D12PipelineState> azdoGeometryPipelineState;
	dx_root_signature azdoGeometryRootSignature;
	ComPtr<ID3D12CommandSignature> azdoCommandSignature;

	ComPtr<ID3D12PipelineState> opaqueGeometryPipelineState;
	dx_root_signature opaqueGeometryRootSignature;

	ComPtr<ID3D12PipelineState> skyPipelineState;
	dx_root_signature skyRootSignature;

	ComPtr<ID3D12PipelineState> directionalLightPipelineState;
	dx_root_signature directionalLightRootSignature;

	ComPtr<ID3D12PipelineState> ambientLightPipelineState;
	dx_root_signature ambientLightRootSignature;

	ComPtr<ID3D12PipelineState> presentPipelineState;
	dx_root_signature presentRootSignature;


	dx_mesh azdoMesh;
	std::vector<submesh_info> azdoSubmeshes;
	std::vector<dx_material> azdoMaterials;
	dx_buffer azdoCommandBuffer;
	ComPtr<ID3D12DescriptorHeap> azdoDescriptorHeap;

	dx_material cerberusMaterial;
	dx_mesh sceneMesh;
	std::vector<submesh_info> sceneSubmeshes;

	dx_mesh skyMesh;
	submesh_info skySubmesh;

	dx_texture cubemap;
	dx_texture irradiance;
	dx_texture prefilteredEnvironment;
	dx_texture brdf;



	uint32 width;
	uint32 height;
	float dt;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	mat4 modelMatrix;
	
	render_camera camera;

	debug_gui gui;

	dx_render_target gbufferRT;
	dx_render_target lightingRT;

	// Render target textures.
	dx_texture albedoAOTexture;
	dx_texture hdrTexture;
	dx_texture normalRoughnessMetalnessTexture;
	dx_texture depthTexture;
};

