#pragma once

#include "common.h"
#include "command_queue.h"
#include "resource.h"
#include "root_signature.h"
#include "math.h"
#include "camera.h"
#include "render_target.h"
#include "material.h"
#include "lighting.h"
#include "debug_gui.h"
#include "debug_display.h"
#include "platform.h"

#include "sky.h"
#include "present.h"

#define INDIRECT_ROOTPARAM_CAMERA			0
#define INDIRECT_ROOTPARAM_MODEL			1
#define INDIRECT_ROOTPARAM_MATERIAL			2
#define INDIRECT_ROOTPARAM_BRDF_TEXTURES	3
#define INDIRECT_ROOTPARAM_ALBEDOS			4
#define INDIRECT_ROOTPARAM_NORMALS			5
#define INDIRECT_ROOTPARAM_ROUGHNESSES		6
#define INDIRECT_ROOTPARAM_METALLICS		7
#define INDIRECT_ROOTPARAM_DIRECTIONAL		8
#define INDIRECT_ROOTPARAM_SHADOWMAPS		9
#define INDIRECT_ROOTPARAM_LIGHTPROBES		10


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

	void renderScene(dx_command_list* commandList, render_camera& camera);


	bool contentLoaded = false;
	ComPtr<ID3D12Device2> device;

	ComPtr<ID3D12PipelineState> indirectGeometryPipelineState;
	dx_root_signature indirectGeometryRootSignature;
	ComPtr<ID3D12CommandSignature> indirectGeometryCommandSignature;

	ComPtr<ID3D12PipelineState> indirectDepthOnlyPipelineState;
	dx_root_signature indirectDepthOnlyRootSignature;
	ComPtr<ID3D12CommandSignature> indirectDepthOnlyCommandSignature;

	
	sky_pipeline sky;
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
	CD3DX12_GPU_DESCRIPTOR_HANDLE brdfOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE shadowCascadesOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE lightProbeOffset;

	dx_mesh sceneMesh;
	std::vector<submesh_info> sceneSubmeshes;

	directional_light sun;
	std::vector<point_light> pointLights;
	dx_structured_buffer pointLightBuffer;

	dx_texture cubemap;
	dx_texture irradiance;
	dx_texture prefilteredEnvironment;
	dx_texture brdf;

	D3D12_CPU_DESCRIPTOR_HANDLE defaultShadowMapSRV;

	vec3 inputMovement;
	float inputSpeedModifier;

	uint32 width;
	uint32 height;
	float dt;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	render_camera camera;
	bool isDebugCamera = false;
	camera_frustum mainCameraFrustum;

	debug_gui gui;
	debug_display debugDisplay;

	dx_render_target lightingRT;
	dx_texture hdrTexture;
	dx_texture depthTexture;

	uint32 lightProbeFaceIndex = 0;
	uint32 lightProbeGlobalIndex = 0;
	bool lightProbeRecording = false;
	bool showLightProbes = false;
	bool showLightProbeConnectivity = false;

	light_probe_system lightProbeSystem;

	dx_render_target sunShadowMapRT[MAX_NUM_SUN_SHADOW_CASCADES];
	dx_texture sunShadowMapTexture[MAX_NUM_SUN_SHADOW_CASCADES];
};

