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
		s.materialIndex += materialOffset;
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
			commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].albedo, stringToWString(mat.albedoName), texture_type_color);
			commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].normal, stringToWString(mat.normalName), texture_type_noncolor);
			commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].roughness, stringToWString(mat.roughnessName), texture_type_noncolor);
			commandList->loadTextureFromFile(indirectMaterials[i + materialOffset].metallic, stringToWString(mat.metallicName), texture_type_noncolor);
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

		command.modelMatrix = transform;
		command.material.textureID = mesh.materialIndex;
		command.material.usageFlags = (USE_ALBEDO_TEXTURE | USE_NORMAL_TEXTURE | USE_ROUGHNESS_TEXTURE | USE_METALLIC_TEXTURE | USE_AO_TEXTURE);
		command.material.albedoTint = vec4(1.f, 1.f, 1.f, 1.f);
		command.material.drawID = i;

		command.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
		command.drawArguments.InstanceCount = 1;
		command.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
		command.drawArguments.BaseVertexLocation = mesh.baseVertex;
		command.drawArguments.StartInstanceLocation = 0;

		depthOnlyCommand.modelMatrix = transform;
		depthOnlyCommand.drawArguments = command.drawArguments;
	}

	numDrawCalls += (uint32)submeshes.size();
}

void indirect_draw_buffer::push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes, vec4 color, float roughness, float metallic,
	const mat4& transform, dx_command_list* commandList)
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

		command.modelMatrix = transform;
		command.material.textureID = 0;
		command.material.usageFlags = 0;
		command.material.albedoTint = color;
		command.material.roughnessOverride = roughness;
		command.material.metallicOverride = metallic;
		command.material.drawID = i;

		command.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
		command.drawArguments.InstanceCount = 1;
		command.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
		command.drawArguments.BaseVertexLocation = mesh.baseVertex;
		command.drawArguments.StartInstanceLocation = 0;

		depthOnlyCommand.modelMatrix = transform;
		depthOnlyCommand.drawArguments = command.drawArguments;
	}

	numDrawCalls += (uint32)submeshes.size();
}

void indirect_draw_buffer::push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes,
	vec4* colors, float* roughnesses, float* metallics, const mat4* transforms, uint32 instanceCount,
	dx_command_list* commandList)
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

			command.modelMatrix = transforms[instance];
			command.material.textureID = 0;
			command.material.usageFlags = 0;
			command.material.albedoTint = colors[instance];
			command.material.roughnessOverride = roughnesses[instance];
			command.material.metallicOverride = metallics[instance];
			command.material.drawID = i;

			command.drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
			command.drawArguments.InstanceCount = 1;
			command.drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
			command.drawArguments.BaseVertexLocation = mesh.baseVertex;
			command.drawArguments.StartInstanceLocation = 0;

			depthOnlyCommand.modelMatrix = transforms[instance];
			depthOnlyCommand.drawArguments = command.drawArguments;
		}
	}

	numDrawCalls += (uint32)submeshes.size() * instanceCount;
}

void indirect_draw_buffer::finish(ComPtr<ID3D12Device2> device, dx_command_list* commandList,
	dx_texture& environment, dx_texture& irradiance, dx_texture& prefilteredEnvironment, dx_texture& brdf,
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

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDesc.NumDescriptors =
			(uint32)indirectMaterials.size() * 4	// Materials.
			+ 3										// PBR Textures.
			+ MAX_NUM_SUN_SHADOW_CASCADES			// Sun shadow map cascades.
			+ 1										// Spot light shadow map.
			+ 3										// Light probes.
			;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		checkResult(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));
		SET_NAME(descriptorHeap, "Indirect descriptor heap");

		uint32 descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptorHeap->GetGPUDescriptorHandleForHeapStart());


		brdfOffset = gpuHandle;
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = environment.resource->GetDesc().Format;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = (uint32)-1; // Use all mips.

			device->CreateShaderResourceView(irradiance.resource.Get(), &srvDesc, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);

			device->CreateShaderResourceView(prefilteredEnvironment.resource.Get(), &srvDesc, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);

			device->CreateShaderResourceView(brdf.resource.Get(), nullptr, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
		shadowMapsOffset = gpuHandle;
		for (uint32 i = 0; i < numSunShadowMapCascades; ++i)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = dx_texture::getReadFormatFromTypeless(sunShadowMapCascades[i].resource->GetDesc().Format);
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			device->CreateShaderResourceView(sunShadowMapCascades[i].resource.Get(), &srvDesc, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
		for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES - numSunShadowMapCascades; ++i)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 0;

			device->CreateShaderResourceView(nullptr, &srvDesc, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = dx_texture::getReadFormatFromTypeless(spotLightShadowMap.resource->GetDesc().Format);
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			device->CreateShaderResourceView(spotLightShadowMap.resource.Get(), &srvDesc, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}

		lightProbeOffset = gpuHandle;
		{
			lightProbeSystem.lightProbePositionBuffer.createShaderResourceView(device, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);

			lightProbeSystem.packedSphericalHarmonicsBuffer.createShaderResourceView(device, cpuHandle);
			//lightProbeSystem.sphericalHarmonicsBuffer.createShaderResourceView(device, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);

			lightProbeSystem.lightProbeTetrahedraBuffer.createShaderResourceView(device, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}

		// I am putting the material textures at the very end of the descriptor heap, since they are variably sized and the shader complains if there
		// is a buffer coming after them.
		albedosOffset = gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			device->CreateShaderResourceView(indirectMaterials[i].albedo.resource.Get(), nullptr, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
		normalsOffset = gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			device->CreateShaderResourceView(indirectMaterials[i].normal.resource.Get(), nullptr, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
		roughnessesOffset = gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			device->CreateShaderResourceView(indirectMaterials[i].roughness.resource.Get(), nullptr, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
		metallicsOffset = gpuHandle;
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			device->CreateShaderResourceView(indirectMaterials[i].metallic.resource.Get(), nullptr, cpuHandle);
			cpuHandle.Offset(descriptorHandleIncrementSize);
			gpuHandle.Offset(descriptorHandleIncrementSize);
		}
	}
}

void indirect_pipeline::initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget, DXGI_FORMAT shadowMapFormat)
{
	PROFILE_BLOCK("Indirect pipeline");

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_indirect_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_indirect_forward_ps.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "LIGHTPROBE_TETRAHEDRON", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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

	CD3DX12_ROOT_PARAMETER1 rootParameters[12];
	rootParameters[INDIRECT_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // Camera.
	rootParameters[INDIRECT_ROOTPARAM_MODEL].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // Model matrix (mat4).
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
	rootSignatureDesc.NumParameters = 2; // Don't need the materials.
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

	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[3];
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	argumentDescs[0].Constant.RootParameterIndex = INDIRECT_ROOTPARAM_MODEL;
	argumentDescs[0].Constant.DestOffsetIn32BitValues = 0;
	argumentDescs[0].Constant.Num32BitValuesToSet = 16;

	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	argumentDescs[1].Constant.RootParameterIndex = INDIRECT_ROOTPARAM_MATERIAL;
	argumentDescs[1].Constant.DestOffsetIn32BitValues = 0;
	argumentDescs[1].Constant.Num32BitValuesToSet = rootParameters[INDIRECT_ROOTPARAM_MATERIAL].Constants.Num32BitValues;

	argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = arraysize(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(indirect_command);

	checkResult(device->CreateCommandSignature(&commandSignatureDesc, geometryRootSignature.rootSignature.Get(),
		IID_PPV_ARGS(&geometryCommandSignature)));
	SET_NAME(geometryCommandSignature, "Indirect Command Signature");


	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	commandSignatureDesc.NumArgumentDescs = 2;
	commandSignatureDesc.ByteStride = sizeof(indirect_depth_only_command);

	checkResult(device->CreateCommandSignature(&commandSignatureDesc, depthOnlyRootSignature.rootSignature.Get(),
		IID_PPV_ARGS(&depthOnlyCommandSignature)));
	SET_NAME(depthOnlyCommandSignature, "Indirect Shadow Command Signature");
}

void indirect_pipeline::render(dx_command_list* commandList, indirect_draw_buffer& indirectBuffer,
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Geometry.");


	commandList->setPipelineState(geometryPipelineState);
	commandList->setGraphicsRootSignature(geometryRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_CAMERA, cameraCBAddress);

	commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, indirectBuffer.descriptorHeap);

	// PBR.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_BRDF_TEXTURES, indirectBuffer.brdfOffset);

	// Materials.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_ALBEDOS, indirectBuffer.albedosOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_NORMALS, indirectBuffer.normalsOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_ROUGHNESSES, indirectBuffer.roughnessesOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_METALLICS, indirectBuffer.metallicsOffset);

	// Sun.
	commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_DIRECTIONAL, sunCBAddress);
	commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_SPOT, spotLightCBAddress);

	// Shadow maps.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_SHADOWMAPS, indirectBuffer.shadowMapsOffset);

	// Light probes.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_LIGHTPROBES, indirectBuffer.lightProbeOffset);

	commandList->setVertexBuffer(0, indirectBuffer.indirectMesh.vertexBuffer);
	commandList->setIndexBuffer(indirectBuffer.indirectMesh.indexBuffer);


	commandList->drawIndirect(
		geometryCommandSignature,
		indirectBuffer.numDrawCalls,
		indirectBuffer.commandBuffer);

	commandList->resetToDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void indirect_pipeline::renderDepthOnly(dx_command_list* commandList, const render_camera& camera, indirect_draw_buffer& indirectBuffer)
{
	commandList->setPipelineState(depthOnlyPipelineState);
	commandList->setGraphicsRootSignature(depthOnlyRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setVertexBuffer(0, indirectBuffer.indirectMesh.vertexBuffer);
	commandList->setIndexBuffer(indirectBuffer.indirectMesh.indexBuffer);

	// This only works, because the vertex shader expects the vp matrix as the first argument.
	commandList->setGraphics32BitConstants(INDIRECT_ROOTPARAM_CAMERA, camera.viewProjectionMatrix);

	commandList->drawIndirect(
		depthOnlyCommandSignature,
		indirectBuffer.numDrawCalls,
		indirectBuffer.depthOnlyCommandBuffer);
}
