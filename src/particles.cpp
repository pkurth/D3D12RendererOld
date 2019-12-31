#include "pch.h"
#include "particles.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void particle_system::initialize(uint32 numParticles, vec3 position, float spawnRate, float maxLifetime, vec4 color)
{
	particles.reserve(numParticles);
	spawnPosition = position;
	this->spawnRate = spawnRate;
	this->maxLifetime = maxLifetime;
	particleSpawnAccumulator = 0.f;
	this->color = color;
}

void particle_system::update(float dt)
{
	PROFILE_FUNCTION();

	uint32 numParticles = (uint32)particles.size();
	for (uint32 i = 0; i < numParticles; ++i)
	{
		particle_data& p = particles[i];
		p.timeAlive += dt;
		if (p.timeAlive >= maxLifetime)
		{
			std::swap(p, particles[numParticles - 1]);
			--numParticles;
			--i;
		}
	}
	particles.resize(numParticles);

	comp_vec gravity = vec3(0.f, -9.81f, 0.f) * dt;
	for (uint32 i = 0; i < numParticles; ++i)
	{
		particle_data& p = particles[i];
		p.position = p.position + 0.5f * gravity * dt + p.velocity * dt;
		p.velocity = p.velocity + gravity;
	}

	particleSpawnAccumulator += spawnRate * dt;
	uint32 spaceLeft = (uint32)particles.capacity() - numParticles;
	uint32 numNewParticles = min((uint32)particleSpawnAccumulator, spaceLeft);

	particleSpawnAccumulator -= numNewParticles;

	for (uint32 i = 0; i < numNewParticles; ++i)
	{
		particle_data p;
		p.position = spawnPosition;
		p.velocity = comp_vec(randomFloat(-1.f, 1.f), randomFloat(-1.f, 1.f), randomFloat(-1.f, 1.f)).normalize();
		p.timeAlive = 0.f;
		particles.push_back(p);
	}
}

void particle_pipeline::initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget)
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/particles_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_flat_ps.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "INSTANCEPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
	};

	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;


	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[PARTICLES_ROOTPARAM_CB].InitAsConstants((sizeof(mat4) + sizeof(vec4)) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);


	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignature.initialize(device, rootSignatureDesc);


	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
	} pipelineStateStream;

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
	pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
	pipelineStateStream.rasterizer = defaultRasterizerDesc;
	pipelineStateStream.blend = additiveBlendDesc;

	CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
	pipelineStateStream.depthStencilDesc = depthDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

	SET_NAME(rootSignature.rootSignature, "Particle Root Signature");
	SET_NAME(pipelineState, "Particle Pipeline");
}

void particle_pipeline::renderParticleSystem(dx_command_list* commandList, const render_camera& camera, const particle_system& particleSystem)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Particles.");

	commandList->setPipelineState(pipelineState);
	commandList->setGraphicsRootSignature(rootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	std::vector<vec3> positions(particleSystem.particles.size());
	for (uint32 i = 0; i < (uint32)particleSystem.particles.size(); ++i)
	{
		positions[i] = particleSystem.particles[i].position;
	}

	float size = 0.3f;
	vec3 right = camera.rotation * vec3(0.5f * size, 0.f, 0.f);
	vec3 up = camera.rotation * vec3(0.f, 0.5f * size, 0.f);

	vec3 vertices[] =
	{
		-right - up,
		right - up,
		-right + up,
		right + up,
	};

	D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(vertices, arraysize(vertices));
	D3D12_VERTEX_BUFFER_VIEW tmpInstanceBuffer = commandList->createDynamicVertexBuffer(positions.data(), (uint32)positions.size());

	struct
	{
		mat4 vp;
		vec4 color;
	} cb =
	{
		camera.viewProjectionMatrix,
		particleSystem.color
	};
	commandList->setGraphics32BitConstants(PARTICLES_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, tmpVertexBuffer);
	commandList->setVertexBuffer(1, tmpInstanceBuffer);

	commandList->draw(arraysize(vertices), (uint32)positions.size(), 0, 0);
}
