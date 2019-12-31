#pragma once

#include "math.h"
#include "root_signature.h"
#include "command_list.h"
#include "camera.h"
#include "texture.h"

struct particle_data
{
	vec3 position;
	float timeAlive;
	vec3 velocity;
	float maxLifetime;
	vec4 color;
	vec2 uv0;
	vec2 uv1;
};

enum particle_property_type
{
	particle_property_type_constant,
	particle_property_type_linear,
	particle_property_type_random,
};

template <typename T>
struct particle_property_constant
{
	T value;

	inline T interpolate(float t) const
	{
		return value;
	}
};

template <typename T>
struct particle_property_linear
{
	T from;
	T to;

	inline T interpolate(float t) const
	{
		return lerp(from, to, t);
	}
};

template <typename T>
struct particle_property_random
{
	T min;
	T max;

	inline T start() const
	{
		T result;
		for (uint32 i = 0; i < arraysize(result.data); ++i)
		{
			result.data[i] = randomFloat(min.data[i], max.data[i]);
		}
		return result;
	}
};

template <>
struct particle_property_random<float>
{
	float min;
	float max;

	inline float start() const
	{
		return randomFloat(min, max);
	}
};

template <typename T>
struct particle_property
{
	particle_property() {}

	particle_property_type type;

	union
	{
		particle_property_constant<T> constant;
		particle_property_linear<T> linear;
		particle_property_random<T> random;
	};

	inline T start() const
	{
		switch (type)
		{
			case particle_property_type_constant: return constant.value;
			case particle_property_type_linear: return linear.from;
			case particle_property_type_random: return random.start();
		}
		return T();
	}

	inline T interpolate(float relativeLifetime, T current) const
	{
		switch (type)
		{
			case particle_property_type_constant: return constant.interpolate(relativeLifetime);
			case particle_property_type_linear: return linear.interpolate(relativeLifetime);
			case particle_property_type_random: return current;
		}
		return T();
	}

	void initializeAsConstant(T c)
	{
		type = particle_property_type_constant;
		constant.value = c;
	}

	void initializeAsLinear(T from, T to)
	{
		type = particle_property_type_linear;
		linear.from = from;
		linear.to = to;
	}

	void initializeAsRandom(T min, T max)
	{
		type = particle_property_type_random;
		random.min = min;
		random.max = max;
	}
};

struct particle_system
{
	void initialize(uint32 numParticles);
	void update(float dt);

	vec3 spawnPosition;
	float spawnRate;
	float gravityFactor;

	particle_property<vec4> color;
	particle_property<float> maxLifetime;
	particle_property<vec3> startVelocity;

	dx_texture_atlas textureAtlas;
	
	float particleSpawnAccumulator;
	std::vector<particle_data> particles;
};

#define PARTICLES_ROOTPARAM_CB		0
#define PARTICLES_ROOTPARAM_TEXTURE 1

struct particle_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget);
	void renderParticleSystem(dx_command_list* commandList, const render_camera& camera, particle_system& particleSystem);

	ComPtr<ID3D12PipelineState> flatPipelineState;
	dx_root_signature flatRootSignature;

	ComPtr<ID3D12PipelineState> texturedPipelineState;
	dx_root_signature texturedRootSignature;
};

