#include "pch.h"
#include "indirect_drawing.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

void indirect_draw_buffer::pushInternal(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info>& submeshes)
{
	PROFILE_FUNCTION();

	uint32 materialOffset = (uint32)indirectMaterials.size();
	uint32 vertexOffset = (uint32)cpuMesh.vertices.size();
	uint32 triangleOffset = (uint32)cpuMesh.triangles.size();

	cpuMesh.append(mesh);

	for (submesh_info& s : submeshes)
	{
		s.baseVertex += vertexOffset;
		s.firstTriangle += triangleOffset;

		uint32 currentMaterialOffset = s.textureID_usageFlags >> 16;
		currentMaterialOffset += materialOffset;
		s.textureID_usageFlags = (s.textureID_usageFlags & 0xFFFF) | (currentMaterialOffset << 16);
	}
}

void indirect_draw_buffer::push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes, std::vector<submesh_material_info>& materialInfos,
	const mat4& transform, dx_command_list* commandList)
{
	PROFILE_FUNCTION();

	pushInternal(mesh, submeshes);

	uint32 materialOffset = (uint32)indirectMaterials.size();
	indirectMaterials.resize(indirectMaterials.size() + materialInfos.size());

	{
		PROFILE_BLOCK("Load indirect textures");

		for (uint32 i = 0; i < materialInfos.size(); ++i)
		{
			const submesh_material_info& mat = materialInfos[i];
			if (mat.albedoName.length() > 0)
			{
				commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].albedo, stringToWString(mat.albedoName), texture_type_color);
			}
			if (mat.normalName.length() > 0)
			{
				commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].normal, stringToWString(mat.normalName), texture_type_noncolor);
			}
			if (mat.roughnessName.length() > 0)
			{
				commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].roughness, stringToWString(mat.roughnessName), texture_type_noncolor);
			}
			if (mat.metallicName.length() > 0)
			{
				commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].metallic, stringToWString(mat.metallicName), texture_type_noncolor);
			}
		}
	}

	uint32 currentSize = (uint32)commands.size();
	commands.resize(commands.size() + submeshes.size());
	depthOnlyCommands.resize(commands.size() + submeshes.size());

	for (uint32 i = 0; i < submeshes.size(); ++i)
	{
		submesh_info mesh = submeshes[i];

		indirect_command& command = commands[i + currentSize];
		indirect_depth_only_command& depthOnlyCommand = depthOnlyCommands[i + currentSize];

		command.material.textureID_usageFlags = mesh.textureID_usageFlags;
		command.material.albedoTint = vec4(1.f, 1.f, 1.f, 1.f);
		command.material.roughnessOverride = 1.f;
		command.material.metallicOverride = 0.f;

		command.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
		command.drawArguments.InstanceCount = 1;
		command.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
		command.drawArguments.BaseVertexLocation = mesh.baseVertex;
		command.drawArguments.StartInstanceLocation = 0;

		depthOnlyCommand.drawArguments = command.drawArguments;
	}

	numDrawCalls += (uint32)submeshes.size();
}

void indirect_draw_buffer::push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes, vec4 color, float roughness, float metallic,
	const mat4& transform)
{
	PROFILE_FUNCTION();

	pushInternal(mesh, submeshes);

	uint32 currentSize = (uint32)commands.size();
	commands.resize(commands.size() + submeshes.size());
	depthOnlyCommands.resize(commands.size() + submeshes.size());

	for (uint32 i = 0; i < submeshes.size(); ++i)
	{
		submesh_info mesh = submeshes[i];

		indirect_command& command = commands[i + currentSize];
		indirect_depth_only_command& depthOnlyCommand = depthOnlyCommands[i + currentSize];

		command.material.textureID_usageFlags = 0;
		command.material.albedoTint = color;
		command.material.roughnessOverride = roughness;
		command.material.metallicOverride = metallic;

		command.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
		command.drawArguments.InstanceCount = 1;
		command.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
		command.drawArguments.BaseVertexLocation = mesh.baseVertex;
		command.drawArguments.StartInstanceLocation = 0;

		depthOnlyCommand.drawArguments = command.drawArguments;
	}

	numDrawCalls += (uint32)submeshes.size();
}

void indirect_draw_buffer::push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes,
	vec4* colors, float* roughnesses, float* metallics, const mat4* transforms, uint32 instanceCount)
{
	PROFILE_FUNCTION();

	pushInternal(mesh, submeshes);

	uint32 currentSize = (uint32)commands.size();
	commands.resize(commands.size() + submeshes.size() * instanceCount);
	depthOnlyCommands.resize(commands.size() + submeshes.size() * instanceCount);

	for (uint32 instance = 0; instance < instanceCount; ++instance)
	{
		for (uint32 i = 0; i < submeshes.size(); ++i)
		{
			submesh_info mesh = submeshes[i];

			indirect_command& command = commands[instance + instanceCount * i + currentSize];
			indirect_depth_only_command& depthOnlyCommand = depthOnlyCommands[instance + instanceCount * i + currentSize];

			command.material.textureID_usageFlags = 0;
			command.material.albedoTint = colors[instance];
			command.material.roughnessOverride = roughnesses[instance];
			command.material.metallicOverride = metallics[instance];

			command.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
			command.drawArguments.InstanceCount = 1;
			command.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
			command.drawArguments.BaseVertexLocation = mesh.baseVertex;
			command.drawArguments.StartInstanceLocation = 0;

			depthOnlyCommand.drawArguments = command.drawArguments;
		}
	}

	numDrawCalls += (uint32)submeshes.size() * instanceCount;
}

void indirect_draw_buffer::finish(ComPtr<ID3D12Device2> device, dx_command_list* commandList,
	dx_texture& irradiance, dx_texture& prefilteredEnvironment, dx_texture& brdf,
	dx_texture* sunShadowMapCascades, uint32 numSunShadowMapCascades,
	dx_texture& spotLightShadowMap,
	light_probe_system& lightProbeSystem)
{
	{
		PROFILE_BLOCK("Upload indirect mesh to GPU");
		indirectMesh.initialize(device, commandList, cpuMesh);

		SET_NAME(indirectMesh.vertexBuffer.resource, "Indirect vertex buffer");
		SET_NAME(indirectMesh.indexBuffer.resource, "Indirect index buffer");
	}

	{
		commandBuffer.initialize(device, commands.data(), numDrawCalls, commandList);
		depthOnlyCommandBuffer.initialize(device, depthOnlyCommands.data(), numDrawCalls, commandList);

		SET_NAME(commandBuffer.resource, "Indirect command buffer");
		SET_NAME(depthOnlyCommandBuffer.resource, "Indirect depth only command buffer");
	}

	{
		PROFILE_BLOCK("Set up indirect descriptor heap");

		descriptors.descriptorHeap.initialize(device, 
			(uint32)indirectMaterials.size() * 4	// Materials.
			+ 3										// PBR Textures.
			+ MAX_NUM_SUN_SHADOW_CASCADES			// Sun shadow map cascades.
			+ 1										// Spot light shadow map.
			+ 3										// Light probes.
		);
		SET_NAME(descriptors.descriptorHeap.descriptorHeap, "Indirect descriptor heap");

		descriptors.brdfOffset = descriptors.descriptorHeap.gpuHandle;
		descriptors.descriptorHeap.pushCubemap(irradiance);
		descriptors.descriptorHeap.pushCubemap(prefilteredEnvironment);
		descriptors.descriptorHeap.push2DTexture(brdf);

		descriptors.shadowMapsOffset = descriptors.descriptorHeap.gpuHandle;
		for (uint32 i = 0; i < numSunShadowMapCascades; ++i)
		{
			descriptors.descriptorHeap.pushDepthTexture(sunShadowMapCascades[i]);
		}
		for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES - numSunShadowMapCascades; ++i)
		{
			descriptors.descriptorHeap.pushNullTexture();
		}
		descriptors.descriptorHeap.pushDepthTexture(spotLightShadowMap);

		descriptors.lightProbeOffset = descriptors.descriptorHeap.gpuHandle;
		descriptors.descriptorHeap.pushStructuredBuffer(lightProbeSystem.lightProbePositionBuffer);
		descriptors.descriptorHeap.pushStructuredBuffer(lightProbeSystem.packedSphericalHarmonicsBuffer);
		descriptors.descriptorHeap.pushStructuredBuffer(lightProbeSystem.lightProbeTetrahedraBuffer);

		// I am putting the material textures at the very end of the descriptor heap, since they are variably sized and the shader complains if there
		// is a buffer coming after them.
		descriptors.albedosOffset = descriptors.descriptorHeap.gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			if (indirectMaterials[i].albedo.resource)
			{
				descriptors.descriptorHeap.push2DTexture(indirectMaterials[i].albedo);
			}
			else
			{
				descriptors.descriptorHeap.pushNullTexture();
			}
		}
		descriptors.normalsOffset = descriptors.descriptorHeap.gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			if (indirectMaterials[i].normal.resource)
			{
				descriptors.descriptorHeap.push2DTexture(indirectMaterials[i].normal);
			}
			else
			{
				descriptors.descriptorHeap.pushNullTexture();
			}
		}
		descriptors.roughnessesOffset = descriptors.descriptorHeap.gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			if (indirectMaterials[i].roughness.resource)
			{
				descriptors.descriptorHeap.push2DTexture(indirectMaterials[i].roughness);
			}
			else
			{
				descriptors.descriptorHeap.pushNullTexture();
			}
		}
		descriptors.metallicsOffset = descriptors.descriptorHeap.gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			if (indirectMaterials[i].metallic.resource)
			{
				descriptors.descriptorHeap.push2DTexture(indirectMaterials[i].metallic);
			}
			else
			{
				descriptors.descriptorHeap.pushNullTexture();
			}
		}
	}
}

void indirect_pipeline::initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget, DXGI_FORMAT shadowMapFormat)
{
	PROFILE_BLOCK("Indirect pipeline");

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_ps.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "LIGHTPROBE_TETRAHEDRON", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },


		{ "MODELMATRIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODELMATRIX", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODELMATRIX", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODELMATRIX", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};


	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 pbrTextures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

	CD3DX12_DESCRIPTOR_RANGE1 albedos(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 1);
	CD3DX12_DESCRIPTOR_RANGE1 normals(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 2);
	CD3DX12_DESCRIPTOR_RANGE1 roughnesses(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 3);
	CD3DX12_DESCRIPTOR_RANGE1 metallics(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 4);

	CD3DX12_DESCRIPTOR_RANGE1 shadowMaps(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_NUM_SUN_SHADOW_CASCADES + 1, 0, 5); // Sun cascades + spot light.

	CD3DX12_DESCRIPTOR_RANGE1 pointLights(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MAX_NUM_SUN_SHADOW_CASCADES, 5);

	CD3DX12_DESCRIPTOR_RANGE1 lightProbes[] =
	{
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 6), // Positions.
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 6), // Spherical harmonics.
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 6), // Tetrahedra.
	};

	CD3DX12_ROOT_PARAMETER1 rootParameters[11];
	rootParameters[INDIRECT_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // Camera.

	rootParameters[INDIRECT_ROOTPARAM_MATERIAL].InitAsConstants(sizeof(material_cb) / sizeof(float), 2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Material.

	// PBR.
	rootParameters[INDIRECT_ROOTPARAM_BRDF_TEXTURES].InitAsDescriptorTable(1, &pbrTextures, D3D12_SHADER_VISIBILITY_PIXEL);

	// Materials.
	rootParameters[INDIRECT_ROOTPARAM_ALBEDOS].InitAsDescriptorTable(1, &albedos, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[INDIRECT_ROOTPARAM_NORMALS].InitAsDescriptorTable(1, &normals, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[INDIRECT_ROOTPARAM_ROUGHNESSES].InitAsDescriptorTable(1, &roughnesses, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[INDIRECT_ROOTPARAM_METALLICS].InitAsDescriptorTable(1, &metallics, D3D12_SHADER_VISIBILITY_PIXEL);

	// Sun.
	rootParameters[INDIRECT_ROOTPARAM_DIRECTIONAL].InitAsConstantBufferView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	// Spot light.
	rootParameters[INDIRECT_ROOTPARAM_SPOT].InitAsConstantBufferView(4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	// Shadow maps.
	rootParameters[INDIRECT_ROOTPARAM_SHADOWMAPS].InitAsDescriptorTable(1, &shadowMaps, D3D12_SHADER_VISIBILITY_PIXEL);

	// Light probes.
	rootParameters[INDIRECT_ROOTPARAM_LIGHTPROBES].InitAsDescriptorTable(arraysize(lightProbes), lightProbes, D3D12_SHADER_VISIBILITY_PIXEL);


	CD3DX12_STATIC_SAMPLER_DESC samplers[] =
	{
		staticLinearWrapSampler(0),
		staticLinearClampSampler(1),
		staticPointBorderComparisonSampler(2),
	};

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = arraysize(samplers);
	geometryRootSignature.initialize(device, rootSignatureDesc, false);


	{
		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS vs;
			CD3DX12_PIPELINE_STATE_STREAM_PS ps;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
#if DEPTH_PREPASS
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencil;
#endif
		} pipelineStateStream;

		pipelineStateStream.rootSignature = geometryRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
		pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;
#if DEPTH_PREPASS
		pipelineStateStream.depthStencil = equalDepthDesc;
#endif

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&geometryPipelineState)));
	}

	// Depth only pass (for depth pre pass and shadow maps).
	rootParameters[INDIRECT_ROOTPARAM_CAMERA].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // VP matrix.
	rootSignatureDesc.NumParameters = 1; // Don't need the materials.
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	depthOnlyRootSignature.initialize(device, rootSignatureDesc);

	{
		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS vs;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = depthOnlyRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.dsvFormat = shadowMapFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&depthOnlyPipelineState)));
	}


	SET_NAME(geometryRootSignature.rootSignature, "Indirect Root Signature");
	SET_NAME(geometryPipelineState, "Indirect Pipeline");
	SET_NAME(depthOnlyRootSignature.rootSignature, "Indirect Shadow Root Signature");
	SET_NAME(depthOnlyPipelineState, "Indirect Shadow Pipeline");


	// Command Signature.

	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2];
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	argumentDescs[0].Constant.RootParameterIndex = INDIRECT_ROOTPARAM_MATERIAL;
	argumentDescs[0].Constant.DestOffsetIn32BitValues = 0;
	argumentDescs[0].Constant.Num32BitValuesToSet = rootParameters[INDIRECT_ROOTPARAM_MATERIAL].Constants.Num32BitValues;

	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = arraysize(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(indirect_command);

	checkResult(device->CreateCommandSignature(&commandSignatureDesc, geometryRootSignature.rootSignature.Get(),
		IID_PPV_ARGS(&geometryCommandSignature)));
	SET_NAME(geometryCommandSignature, "Indirect Command Signature");


	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.ByteStride = sizeof(indirect_depth_only_command);

	checkResult(device->CreateCommandSignature(&commandSignatureDesc, nullptr, //depthOnlyRootSignature.rootSignature.Get(),
		IID_PPV_ARGS(&depthOnlyCommandSignature)));
	SET_NAME(depthOnlyCommandSignature, "Indirect Shadow Command Signature");
}

void indirect_pipeline::render(dx_command_list* commandList, indirect_draw_buffer& indirectBuffer,
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress)
{
	render(commandList, indirectBuffer.indirectMesh, indirectBuffer.descriptors,
		indirectBuffer.commandBuffer, indirectBuffer.numDrawCalls, cameraCBAddress, sunCBAddress, spotLightCBAddress);
}

void indirect_pipeline::render(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
	dx_buffer& commandBuffer, uint32 maxNumDrawCalls, dx_buffer& numDrawCallsBuffer, dx_vertex_buffer& instanceBuffer,
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress, D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Draw indirect.");

	setupPipeline(commandList, mesh, descriptors, commandBuffer, cameraCBAddress, sunCBAddress, spotLightCBAddress);

	commandList->setVertexBuffer(1, instanceBuffer);

	commandList->drawIndirect(
		geometryCommandSignature,
		maxNumDrawCalls, numDrawCallsBuffer,
		commandBuffer);

	commandList->resetToDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void indirect_pipeline::render(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
	dx_buffer& commandBuffer, uint32 numDrawCalls,
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress, D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Draw indirect.");

	setupPipeline(commandList, mesh, descriptors, commandBuffer, cameraCBAddress, sunCBAddress, spotLightCBAddress);

	commandList->drawIndirect(
		geometryCommandSignature,
		numDrawCalls,
		commandBuffer);

	commandList->resetToDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void indirect_pipeline::renderDepthOnly(dx_command_list* commandList, const render_camera& camera, indirect_draw_buffer& indirectBuffer)
{
	renderDepthOnly(commandList, camera, indirectBuffer.indirectMesh, indirectBuffer.depthOnlyCommandBuffer, indirectBuffer.numDrawCalls);
}

void indirect_pipeline::renderDepthOnly(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh,
	dx_buffer& depthOnlyCommandBuffer, uint32 maxNumDrawCalls, dx_buffer& numDrawCallsBuffer, dx_vertex_buffer& instanceBuffer)
{
	setupDepthOnlyPipeline(commandList, camera, mesh, depthOnlyCommandBuffer);

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Draw depth only indirect.");

	commandList->setVertexBuffer(1, instanceBuffer);

	commandList->drawIndirect(
		depthOnlyCommandSignature,
		maxNumDrawCalls, numDrawCallsBuffer,
		depthOnlyCommandBuffer);
}

void indirect_pipeline::renderDepthOnly(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh,
	dx_buffer& depthOnlyCommandBuffer, uint32 numDrawCalls)
{
	setupDepthOnlyPipeline(commandList, camera, mesh, depthOnlyCommandBuffer);

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Draw depth only indirect.");
	
	commandList->drawIndirect(
		depthOnlyCommandSignature,
		numDrawCalls,
		depthOnlyCommandBuffer);
}

void indirect_pipeline::setupDepthOnlyPipeline(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh, dx_buffer& commandBuffer)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Setup depth only indirect pipeline.");

	commandList->setPipelineState(depthOnlyPipelineState);
	commandList->setGraphicsRootSignature(depthOnlyRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);

	// This only works, because the vertex shader expects the vp matrix as the first argument.
	commandList->setGraphics32BitConstants(INDIRECT_ROOTPARAM_CAMERA, camera.viewProjectionMatrix);

}

void indirect_pipeline::setupPipeline(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
	dx_buffer& commandBuffer,
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Setup indirect pipeline.");


	commandList->setPipelineState(geometryPipelineState);
	commandList->setGraphicsRootSignature(geometryRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_CAMERA, cameraCBAddress);

	commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptors.descriptorHeap.descriptorHeap);

	// PBR.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_BRDF_TEXTURES, descriptors.brdfOffset);

	// Materials.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_ALBEDOS, descriptors.albedosOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_NORMALS, descriptors.normalsOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_ROUGHNESSES, descriptors.roughnessesOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_METALLICS, descriptors.metallicsOffset);

	// Sun.
	commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_DIRECTIONAL, sunCBAddress);
	commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_SPOT, spotLightCBAddress);

	// Shadow maps.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_SHADOWMAPS, descriptors.shadowMapsOffset);

	// Light probes.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_LIGHTPROBES, descriptors.lightProbeOffset);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);

}
