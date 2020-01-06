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
#include "particles.h"
#include "indirect_drawing.h"

#include "sky.h"
#include "present.h"
#include "procedural_placement.h"
#include "procedural_placement_editor.h"


#define CAMERA_SENSITIVITY 4.f
#define CAMERA_MOVEMENT_SPEED 10.f

class dx_game
{
public:
	void initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height, color_depth colorDepth = color_depth_8);
	void resize(uint32 width, uint32 height);

	void update(float dt);
	uint64 render(ComPtr<ID3D12Resource> backBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV);

	bool keyDownCallback(keyboard_event event);
	bool keyUpCallback(keyboard_event event);
	bool mouseMoveCallback(mouse_move_event event);

private:

	void renderScene(dx_command_list* commandList, render_camera& camera);
	void renderShadowmap(dx_command_list* commandList, dx_render_target& shadowMapRT, const mat4& vp);


	bool contentLoaded = false;
	ComPtr<ID3D12Device2> device;

	indirect_pipeline indirect;
	particle_pipeline particles;
	sky_pipeline sky;
	present_pipeline present;

	procedural_placement proceduralPlacement;

	procedural_placement_editor proceduralPlacementEditor;


	particle_system particleSystem1;
	particle_system particleSystem2;
	particle_system particleSystem3;
	float particleSystemTime = 0.f;

	indirect_draw_buffer indirectBuffer;

	dx_mesh sceneMesh;
	std::vector<submesh_info> sceneSubmeshes;

	directional_light sun;
	spot_light spotLight;

	dx_texture cubemap;
	dx_texture irradiance;
	dx_texture prefilteredEnvironment;
	dx_texture brdf;

	vec3 inputMovement;
	float inputSpeedModifier;

	uint32 width;
	uint32 height;
	float dt;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	render_camera camera;
	render_camera mainCameraCopy;
	bool isDebugCamera = false;
	camera_frustum_corners mainCameraFrustum;

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

	dx_render_target spotLightShadowMapRT;
	dx_texture spotLightShadowMapTexture;
};

