#include "pch.h"
#include "particles.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void particle_system::initialize(uint32 numParticles)
{
	particles.reserve(numParticles);
	particleSpawnAccumulator = 0.f;
}

void particle_system::update(float dt)
{
	PROFILE_FUNCTION();

	uint32 numParticles = (uint32)particles.size();
	for (uint32 i = 0; i < numParticles; ++i)
	{
		particle_data& p = particles[i];
		p.timeAlive += dt;
		if (p.timeAlive >= p.maxLifetime)
		{
			std::swap(p, particles[numParticles - 1]);
			--numParticles;
			--i;
		}
	}
	particles.resize(numParticles);

	comp_vec gravity(0.f, -9.81f * gravityFactor * dt, 0.f, 0.f);
	uint32 textureSlices = textureAtlas.slicesX * textureAtlas.slicesY;
	for (uint32 i = 0; i < numParticles; ++i)
	{
		particle_data& p = particles[i];
		p.position = p.position + 0.5f * gravity * dt + p.velocity * dt;
		p.velocity = p.velocity + gravity;
		float relLifetime = p.timeAlive / p.maxLifetime;
		p.color = color.interpolate(relLifetime, p.color);
		if (textureAtlas.resource)
		{
			uint32 index = min((uint32)(relLifetime * textureSlices), textureSlices - 1);
			textureAtlas.getUVs(index, p.uv0, p.uv1);
		}
	}

	particleSpawnAccumulator += spawnRate * dt;
	uint32 spaceLeft = (uint32)particles.capacity() - numParticles;
	uint32 numNewParticles = min((uint32)particleSpawnAccumulator, spaceLeft);

	particleSpawnAccumulator -= numNewParticles;

	for (uint32 i = 0; i < numNewParticles; ++i)
	{
		particle_data p;
		p.position = spawnPosition;
		p.timeAlive = 0.f;
		p.velocity = startVelocity.start();
		p.color = color.start();
		p.maxLifetime = maxLifetime.start();
		if (textureAtlas.resource)
		{
			textureAtlas.getUVs(0, p.uv0, p.uv1);
		}
		
		particles.push_back(p);
	}
}

void particle_pipeline::initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget)
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/particles_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> texturedPixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_textured_ps.cso", &texturedPixelShaderBlob));
	ComPtr<ID3DBlob> flatPixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_flat_ps.cso", &flatPixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "INSTANCEPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
		{ "INSTANCECOLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		// We are using the semantic index here to append 0 or 1 to the name.
		{ "INSTANCETEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "INSTANCETEXCOORDS", 1, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};

	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[PARTICLES_ROOTPARAM_CB].InitAsConstants(sizeof(mat4) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[PARTICLES_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	texturedRootSignature.initialize(device, rootSignatureDesc);


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

	pipelineStateStream.rootSignature = texturedRootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(texturedPixelShaderBlob.Get());
	pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
	pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
	pipelineStateStream.rasterizer = defaultRasterizerDesc;
	pipelineStateStream.blend = additiveBlendDesc;

	CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
	pipelineStateStream.depthStencilDesc = depthDesc;

	{
		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&texturedPipelineState)));
	}
	SET_NAME(texturedRootSignature.rootSignature, "Textured Particle Root Signature");
	SET_NAME(texturedPipelineState, "Textured Particle Pipeline");



	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.NumStaticSamplers = 0;
	flatRootSignature.initialize(device, rootSignatureDesc);

	pipelineStateStream.rootSignature = flatRootSignature.rootSignature.Get();
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(flatPixelShaderBlob.Get());

	{
		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&flatPipelineState)));
	}
	SET_NAME(flatRootSignature.rootSignature, "Flat Particle Root Signature");
	SET_NAME(flatPipelineState, "Flat Particle Pipeline");
}

struct particle_instance_data
{
	vec3 position;
	vec4 color;
	vec2 uv0;
	vec2 uv1;
};

void particle_pipeline::renderParticleSystem(dx_command_list* commandList, const render_camera& camera, particle_system& particleSystem)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Particles.");

	if (particleSystem.textureAtlas.resource)
	{
		commandList->setPipelineState(texturedPipelineState);
		commandList->setGraphicsRootSignature(texturedRootSignature);

		commandList->setShaderResourceView(PARTICLES_ROOTPARAM_TEXTURE, 0, particleSystem.textureAtlas);
	}
	else
	{
		commandList->setPipelineState(flatPipelineState);
		commandList->setGraphicsRootSignature(flatRootSignature);
	}

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	std::vector<particle_instance_data> instanceData(particleSystem.particles.size());
	for (uint32 i = 0; i < (uint32)particleSystem.particles.size(); ++i)
	{
		instanceData[i].position = particleSystem.particles[i].position;
		instanceData[i].color = particleSystem.particles[i].color;
		instanceData[i].uv0 = particleSystem.particles[i].uv0;
		instanceData[i].uv1 = particleSystem.particles[i].uv1;
	}

	float size = 0.3f;
	vec3 right = camera.rotation * vec3(0.5f * size, 0.f, 0.f);
	vec3 up = camera.rotation * vec3(0.f, 0.5f * size, 0.f);

	vertex_3PU vertices[] =
	{
		{ -right - up, vec2(0.f, 0.f) },
		{ right - up,  vec2(1.f, 0.f) },
		{ -right + up, vec2(0.f, 1.f) },
		{ right + up,  vec2(1.f, 1.f) },
	};

	D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(vertices, arraysize(vertices));
	D3D12_VERTEX_BUFFER_VIEW tmpInstanceBuffer = commandList->createDynamicVertexBuffer(instanceData.data(), (uint32)instanceData.size());

	struct
	{
		mat4 vp;
	} cb =
	{
		camera.viewProjectionMatrix
	};
	commandList->setGraphics32BitConstants(PARTICLES_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, tmpVertexBuffer);
	commandList->setVertexBuffer(1, tmpInstanceBuffer);

	commandList->draw(arraysize(vertices), (uint32)instanceData.size(), 0, 0);
}
