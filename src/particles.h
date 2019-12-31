#pragma once

#include "math.h"
#include "root_signature.h"
#include "command_list.h"
#include "camera.h"

struct particle_data
{
	vec3 position;
	vec3 velocity;
	float timeAlive;
};

struct particle_system
{
	void initialize(uint32 numParticles, vec3 position, float spawnRate, float maxLifetime, vec4 color);
	void update(float dt);

	vec3 spawnPosition;
	float spawnRate;
	float maxLifetime;
	float particleSpawnAccumulator;
	vec4 color;
	std::vector<particle_data> particles;
};

#define PARTICLES_ROOTPARAM_CB 0

struct particle_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget);
	void renderParticleSystem(dx_command_list* commandList, const render_camera& camera, const particle_system& particleSystem);

	ComPtr<ID3D12PipelineState> pipelineState;
	dx_root_signature rootSignature;
};

