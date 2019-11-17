#pragma once

#include "common.h"
#include "command_queue.h"
#include "resource.h"
#include "root_signature.h"
#include "math.h"
#include "camera.h"
#include "render_target.h"
#include "material.h"
#include "light.h"
#include "debug_gui.h"
#include "platform.h"

#include "sky.h"
#include "lighting.h"
#include "present.h"

#define INDIRECT_ROOTPARAM_CAMERA			0
#define INDIRECT_ROOTPARAM_MODEL			1
#define INDIRECT_ROOTPARAM_MATERIAL			2
#define INDIRECT_ROOTPARAM_PBR_TEXTURES		3
#define INDIRECT_ROOTPARAM_ALBEDOS			4
#define INDIRECT_ROOTPARAM_NORMALS			5
#define INDIRECT_ROOTPARAM_ROUGHNESSES		6
#define INDIRECT_ROOTPARAM_METALLICS		7
#define INDIRECT_ROOTPARAM_DIRECTIONAL		8
#define INDIRECT_ROOTPARAM_SHADOWMAPS		9


#define CAMERA_SENSITIVITY 4.f
#define CAMERA_MOVEMENT_SPEED 10.f

class dx_game
{
public:
	void initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height, color_depth colorDepth = color_depth_8);
	void resize(uint32 width, uint32 height);

	void update(float dt);
	void render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV);

	bool keyDownCallback(keyboard_event event);
	bool keyUpCallback(keyboard_event event);
	bool mouseMoveCallback(mouse_move_event event);

private:

	bool contentLoaded = false;
	ComPtr<ID3D12Device2> device;

	ComPtr<ID3D12PipelineState> indirectGeometryPipelineState;
	dx_root_signature indirectGeometryRootSignature;
	ComPtr<ID3D12CommandSignature> indirectGeometryCommandSignature;

	ComPtr<ID3D12PipelineState> indirectDepthOnlyPipelineState;
	dx_root_signature indirectDepthOnlyRootSignature;
	ComPtr<ID3D12CommandSignature> indirectDepthOnlyCommandSignature;

	
	sky_pipeline sky;
	lighting_pipeline lighting;
	present_pipeline present;


	dx_mesh indirectMesh;
	std::vector<dx_material> indirectMaterials;
	dx_buffer indirectCommandBuffer;
	dx_buffer indirectDepthOnlyCommandBuffer;
	uint32 numIndirectDrawCalls;
	ComPtr<ID3D12DescriptorHeap> indirectDescriptorHeap;
	CD3DX12_GPU_DESCRIPTOR_HANDLE albedosOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE normalsOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE roughnessesOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE metallicsOffset;

	dx_mesh sceneMesh;
	std::vector<submesh_info> sceneSubmeshes;

	directional_light sun;

	dx_texture cubemap;
	dx_texture irradiance;
	dx_texture prefilteredEnvironment;

	vec3 inputMovement;
	float inputSpeedModifier;

	uint32 width;
	uint32 height;
	float dt;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	mat4 modelMatrix;
	
	render_camera camera;

	debug_gui gui;

	dx_render_target lightingRT;
	dx_texture hdrTexture;
	dx_texture depthTexture;

	dx_render_target sunShadowMapRT[MAX_NUM_SUN_SHADOW_CASCADES];
	dx_texture sunShadowMapTexture[MAX_NUM_SUN_SHADOW_CASCADES];
};

