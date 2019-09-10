#include "pch.h"
#include "game.h"
#include "error.h"
#include "model.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

#pragma pack(push, 1)
struct indirect_command
{
	mat4 modelMatrix;
	material_cb material;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	uint32 padding[2];
};

struct indirect_shadow_command
{
	mat4 modelMatrix;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	uint32 padding[3];
};
#pragma pack(pop)

void dx_game::initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height, color_depth colorDepth)
{
	this->device = device;
	scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);

	D3D12_RT_FORMAT_ARRAY screenRTFormats = {};
	screenRTFormats.NumRenderTargets = 1;
	if (colorDepth == color_depth_8)
	{
		screenRTFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else
	{
		assert(colorDepth == color_depth_10);
		screenRTFormats.RTFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	}

	{
		PROFILE_BLOCK("Render targets");

		// Albedo, AO.
		{
			DXGI_FORMAT albedoAOFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			CD3DX12_RESOURCE_DESC albedoAOTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(albedoAOFormat, width, height);
			albedoAOTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE albedoClearValue;
			albedoClearValue.Format = albedoAOTextureDesc.Format;
			albedoClearValue.Color[0] = 0.f;
			albedoClearValue.Color[1] = 0.f;
			albedoClearValue.Color[2] = 0.f;
			albedoClearValue.Color[3] = 0.f;

			albedoAOTexture.initialize(device, albedoAOTextureDesc, &albedoClearValue);
			gbufferRT.attachColorTexture(0, albedoAOTexture);
		}

		// Emission.
		{
			DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
			CD3DX12_RESOURCE_DESC hdrTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrFormat, width, height);
			hdrTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE hdrClearValue;
			hdrClearValue.Format = hdrTextureDesc.Format;
			hdrClearValue.Color[0] = 0.f;
			hdrClearValue.Color[1] = 0.f;
			hdrClearValue.Color[2] = 0.f;
			hdrClearValue.Color[3] = 0.f;

			hdrTexture.initialize(device, hdrTextureDesc, &hdrClearValue);
			gbufferRT.attachColorTexture(1, hdrTexture);
			lightingRT.attachColorTexture(0, hdrTexture);
		}

		// Normals, roughness, metalness.
		{
			DXGI_FORMAT normalRoughnessMetalnessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
			CD3DX12_RESOURCE_DESC normalRoughnessMetalnessTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(normalRoughnessMetalnessFormat, width, height);
			normalRoughnessMetalnessTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE normalClearValue;
			normalClearValue.Format = normalRoughnessMetalnessTextureDesc.Format;
			normalClearValue.Color[0] = 0.f;
			normalClearValue.Color[1] = 0.f;
			normalClearValue.Color[2] = 0.f;
			normalClearValue.Color[3] = 0.f;

			normalRoughnessMetalnessTexture.initialize(device, normalRoughnessMetalnessTextureDesc, &normalClearValue);
			gbufferRT.attachColorTexture(2, normalRoughnessMetalnessTexture);
		}

		// Depth.
		{
			DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // We need stencil for deferred lighting.
			CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, width, height);
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			depthDesc.MipLevels = 1;

			D3D12_CLEAR_VALUE depthClearValue;
			depthClearValue.Format = depthDesc.Format;
			depthClearValue.DepthStencil = { 1.f, 0 };

			depthTexture.initialize(device, depthDesc, &depthClearValue);
			depthTextureCopy.initialize(device, depthDesc, &depthClearValue);

			gbufferRT.attachDepthStencilTexture(depthTexture);
			lightingRT.attachDepthStencilTexture(depthTexture);
		}

		// Sun shadow map.
		{
			DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
			CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, sun.shadowMapDimensions, sun.shadowMapDimensions);
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			depthDesc.MipLevels = 1;

			D3D12_CLEAR_VALUE depthClearValue;
			depthClearValue.Format = depthBufferFormat;
			depthClearValue.DepthStencil = { 1.f, 0 };

			for (uint32 i = 0; i < sun.numShadowCascades; ++i)
			{
				sunShadowMapTexture[i].initialize(device, depthDesc, &depthClearValue);
				sunShadowMapRT[i].attachDepthStencilTexture(sunShadowMapTexture[i]);
			}
		}
	}


	// Indirect.
	{
		PROFILE_BLOCK("Indirect pipeline");

		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_indirect_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_indirect_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};


		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 albedos(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 0);
		CD3DX12_DESCRIPTOR_RANGE1 normals(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 1);
		CD3DX12_DESCRIPTOR_RANGE1 roughnesses(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 2);
		CD3DX12_DESCRIPTOR_RANGE1 metallics(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 3);


		CD3DX12_ROOT_PARAMETER1 rootParameters[7];
		rootParameters[INDIRECT_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Camera.
		rootParameters[INDIRECT_ROOTPARAM_MODEL].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // Model matrix (mat4).
		rootParameters[INDIRECT_ROOTPARAM_MATERIAL].InitAsConstants(sizeof(material_cb) / sizeof(float), 2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Material.
		rootParameters[INDIRECT_ROOTPARAM_ALBEDOS].InitAsDescriptorTable(1, &albedos, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[INDIRECT_ROOTPARAM_NORMALS].InitAsDescriptorTable(1, &normals, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[INDIRECT_ROOTPARAM_ROUGHNESSES].InitAsDescriptorTable(1, &roughnesses, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[INDIRECT_ROOTPARAM_METALLICS].InitAsDescriptorTable(1, &metallics, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearWrapSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		indirectGeometryRootSignature.initialize(device, rootSignatureDesc);


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
				CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
			} pipelineStateStream;

			pipelineStateStream.rootSignature = indirectGeometryRootSignature.rootSignature.Get();
			pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
			pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			pipelineStateStream.dsvFormat = gbufferRT.depthStencilFormat;
			pipelineStateStream.rtvFormats = gbufferRT.renderTargetFormat;
			pipelineStateStream.rasterizer = defaultRasterizerDesc;
			pipelineStateStream.depthStencilDesc = alwaysReplaceStencilDesc;

			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(pipeline_state_stream), &pipelineStateStream
			};
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&indirectGeometryPipelineState)));
		}

		// Shadow pass.
		rootParameters[INDIRECT_ROOTPARAM_CAMERA].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // VP matrix.
		rootSignatureDesc.NumParameters = 2; // Don't need the materials.
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		indirectShadowRootSignature.initialize(device, rootSignatureDesc);

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

			pipelineStateStream.rootSignature = indirectShadowRootSignature.rootSignature.Get();
			pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
			pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.dsvFormat = sunShadowMapRT[0].depthStencilFormat;
			pipelineStateStream.rasterizer = defaultRasterizerDesc;

			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(pipeline_state_stream), &pipelineStateStream
			};
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&indirectShadowPipelineState)));
		}


		SET_NAME(indirectGeometryRootSignature.rootSignature, "Indirect Root Signature");
		SET_NAME(indirectGeometryPipelineState, "Indirect Pipeline");
		SET_NAME(indirectShadowRootSignature.rootSignature, "Indirect Shadow Root Signature");
		SET_NAME(indirectShadowPipelineState, "Indirect Shadow Pipeline");


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

		checkResult(device->CreateCommandSignature(&commandSignatureDesc, indirectGeometryRootSignature.rootSignature.Get(),
			IID_PPV_ARGS(&indirectGeometryCommandSignature)));
		SET_NAME(indirectGeometryCommandSignature, "Indirect Command Signature");


		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		commandSignatureDesc.NumArgumentDescs = 2;
		commandSignatureDesc.ByteStride = sizeof(indirect_shadow_command);

		checkResult(device->CreateCommandSignature(&commandSignatureDesc, indirectShadowRootSignature.rootSignature.Get(),
			IID_PPV_ARGS(&indirectShadowCommandSignature)));
		SET_NAME(indirectShadowCommandSignature, "Indirect Shadow Command Signature");
	}

	// Geometry. This writes to the GBuffer.
	{
		PROFILE_BLOCK("Geometry pipeline");

		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/geometry_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};


		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[GEOMETRY_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Camera.
		rootParameters[GEOMETRY_ROOTPARAM_MODEL].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // Model matrix (mat4).
		rootParameters[GEOMETRY_ROOTPARAM_TEXTURES].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL); // Material textures.

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearWrapSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		opaqueGeometryRootSignature.initialize(device, rootSignatureDesc);



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
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = opaqueGeometryRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = gbufferRT.depthStencilFormat;
		pipelineStateStream.rtvFormats = gbufferRT.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;
		pipelineStateStream.depthStencilDesc = alwaysReplaceStencilDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&opaqueGeometryPipelineState)));

		SET_NAME(opaqueGeometryRootSignature.rootSignature, "Opaque Root Signature");
		SET_NAME(opaqueGeometryPipelineState, "Opaque Pipeline");
	}


	dx_command_queue& copyCommandQueue = dx_command_queue::copyCommandQueue;
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();

	{
		PROFILE_BLOCK("Sky, lighting, present pipeline");
		sky.initialize(device, commandList, lightingRT);
		lighting.initialize(device, commandList, lightingRT);
		present.initialize(device, screenRTFormats);
	}

	{
		PROFILE_BLOCK("Init gui");
		gui.initialize(device, commandList, screenRTFormats);
	}

	{
		PROFILE_BLOCK("Load sponza model");

		std::vector<submesh_material_info> sponzaMaterials;

		{
			PROFILE_BLOCK("Load sponza mesh");

			cpu_mesh<vertex_3PUNT> indirect;
			auto [sponzaSubmeshes, sponzaMaterials_] = indirect.pushFromFile("res/sponza/sponza.obj");
			append(indirectSubmeshes, sponzaSubmeshes);

			{
				PROFILE_BLOCK("Upload to GPU");
				indirectMesh.initialize(device, commandList, indirect);
			}

			sponzaMaterials = std::move(sponzaMaterials_);
		}

		{
			PROFILE_BLOCK("Load sponza textures");

			indirectMaterials.resize(sponzaMaterials.size());
			for (uint32 i = 0; i < sponzaMaterials.size(); ++i)
			{
				commandList->loadTextureFromFile(indirectMaterials[i].albedo, stringToWString(sponzaMaterials[i].albedoName), texture_type_color);
				commandList->loadTextureFromFile(indirectMaterials[i].normal, stringToWString(sponzaMaterials[i].normalName), texture_type_noncolor);
				commandList->loadTextureFromFile(indirectMaterials[i].roughness, stringToWString(sponzaMaterials[i].roughnessName), texture_type_noncolor);
				commandList->loadTextureFromFile(indirectMaterials[i].metallic, stringToWString(sponzaMaterials[i].metallicName), texture_type_noncolor);
			}
		}
		{
			PROFILE_BLOCK("Set up descriptor heap");

			D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
			descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descriptorHeapDesc.NumDescriptors = (uint32)indirectMaterials.size() * 4;
			descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			checkResult(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&indirectDescriptorHeap)));

			uint32 descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(indirectDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(indirectDescriptorHeap->GetGPUDescriptorHandleForHeapStart());


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

		{
			PROFILE_BLOCK("Set up indirect command buffers");

			indirect_command* indirectCommands = new indirect_command[indirectSubmeshes.size()];
			indirect_shadow_command* indirectShadowCommands = new indirect_shadow_command[indirectSubmeshes.size()];

			mat4 model = createScaleMatrix(0.03f);

			for (uint32 i = 0; i < indirectSubmeshes.size(); ++i)
			{
				indirectCommands[i].modelMatrix = model;
				indirectShadowCommands[i].modelMatrix = model;

				uint32 id = i;

				submesh_info mesh = indirectSubmeshes[id];

				indirectCommands[i].material.textureID = mesh.materialIndex;
				indirectCommands[i].material.usageFlags = (USE_ALBEDO_TEXTURE | USE_NORMAL_TEXTURE | USE_ROUGHNESS_TEXTURE | USE_METALLIC_TEXTURE | USE_AO_TEXTURE);
				indirectCommands[i].material.albedoTint = vec4(1.f, 1.f, 1.f, 1.f);
				indirectCommands[i].material.drawID = i;

				indirectCommands[i].drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
				indirectCommands[i].drawArguments.InstanceCount = 1;
				indirectCommands[i].drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
				indirectCommands[i].drawArguments.BaseVertexLocation = mesh.baseVertex;
				indirectCommands[i].drawArguments.StartInstanceLocation = 0;

				indirectShadowCommands[i].drawArguments = indirectCommands[i].drawArguments;
			}

			indirectCommandBuffer.initialize(device, indirectCommands, (uint32)indirectSubmeshes.size(), commandList);
			indirectShadowCommandBuffer.initialize(device, indirectShadowCommands, (uint32)indirectSubmeshes.size(), commandList);

			delete[] indirectShadowCommands;
			delete[] indirectCommands;
		}

	}


	sun.worldSpaceDirection = comp_vec(-0.6f, -1.f, -0.3f, 0.f).normalize();
	sun.color = vec4(1.f, 0.93f, 0.76f, 0.f) * 3.f;
	//sun.color = vec4(1.f, 0, 0, 1.f) * 8.f;

	{
		PROFILE_BLOCK("Load environment");

		dx_texture equirectangular;
		{
			PROFILE_BLOCK("Load HDRI");
			commandList->loadTextureFromFile(equirectangular, L"res/hdris/sunset_in_the_chalk_quarry_4k_16bit.hdr", texture_type_noncolor);
		}
		{
			PROFILE_BLOCK("Convert to cubemap");
			commandList->convertEquirectangularToCubemap(equirectangular, cubemap, 1024, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		}
		{
			PROFILE_BLOCK("Create irradiance map");
			commandList->createIrradianceMap(cubemap, irradiance);
		}
		{
			PROFILE_BLOCK("Prefilter environment");
			commandList->prefilterEnvironmentMap(cubemap, prefilteredEnvironment, 256);
		}
	}

	{
		PROFILE_BLOCK("Execute copy command list");

		uint64 fenceValue = copyCommandQueue.executeCommandList(commandList);
		copyCommandQueue.waitForFenceValue(fenceValue);
	}

	{
		PROFILE_BLOCK("Transition textures to resource state");

		dx_command_queue& renderCommandQueue = dx_command_queue::renderCommandQueue;
		commandList = renderCommandQueue.getAvailableCommandList();


		// Transition indirect textures to pixel shader state.
		for (uint32 i = 0; i < indirectMaterials.size(); ++i)
		{
			commandList->transitionBarrier(indirectMaterials[i].albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->transitionBarrier(indirectMaterials[i].normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->transitionBarrier(indirectMaterials[i].roughness, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->transitionBarrier(indirectMaterials[i].metallic, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		
		{
			PROFILE_BLOCK("Execute transition command list");

			uint64 fenceValue = renderCommandQueue.executeCommandList(commandList);
			renderCommandQueue.waitForFenceValue(fenceValue);
		}
	}

	// Loading scene done.
	contentLoaded = true;

	this->width = width;
	this->height = height;
	viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);
	flushApplication();

	camera.fovY = DirectX::XMConvertToRadians(70.f);
	camera.nearPlane = 0.1f;
	camera.farPlane = 100.f;
	camera.position = vec3(0.f, 5.f, 5.f);
	camera.rotation = quat::identity;
	camera.updateMatrices(width, height);

	inputMovement = vec3(0.f, 0.f, 0.f);
	inputSpeedModifier = 1.f;

	registerKeyDownCallback(BIND(keyDownCallback));
	registerKeyUpCallback(BIND(keyUpCallback));
	registerMouseMoveCallback(BIND(mouseMoveCallback));
}

void dx_game::resize(uint32 width, uint32 height)
{
	if (width != this->width || height != this->height)
	{
		width = max(1u, width);
		height = max(1u, height);

		this->width = width;
		this->height = height;
		viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);

		flushApplication();

		gbufferRT.resize(width, height);
		lightingRT.resize(width, height);

		depthTextureCopy.resize(width, height);
	}
}

void dx_game::update(float dt)
{
	camera.rotation = createQuaternionFromAxisAngle(comp_vec(0.f, 1.f, 0.f), camera.yaw)
		* createQuaternionFromAxisAngle(comp_vec(1.f, 0.f, 0.f), camera.pitch);

	camera.position = camera.position + camera.rotation * inputMovement * dt * CAMERA_MOVEMENT_SPEED * inputSpeedModifier;
	camera.updateMatrices(width, height);

	sun.updateMatrices(camera.getWorldSpaceFrustum());

	this->dt = dt;

	DEBUG_TAB(gui, "Stats")
	{
		gui.textF("Performance: %.2f fps (%.3f ms)", 1.f / dt, dt * 1000.f);
		DEBUG_GROUP(gui, "Camera")
		{
			gui.textF("Camera position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
			gui.textF("Input movement: %.2f, %.2f, %.2f", inputMovement.x, inputMovement.y, inputMovement.z);
		}
		gui.textF("%u draw calls", (uint32)indirectSubmeshes.size());
	}
}

void dx_game::render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV)
{
	PIXSetMarker(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 0, 0), "Frame start.");

	camera_cb cameraCB;
	camera.fillConstantBuffer(cameraCB);

	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress = commandList->uploadDynamicConstantBuffer(cameraCB);
	D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress = commandList->uploadDynamicConstantBuffer(sun);


	commandList->setScissor(scissorRect);


	// Render to sun shadow map.
	{
		PROFILE_BLOCK("Record shadow map commands");

		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Sun shadow map.");

		commandList->setViewport(sunShadowMapRT[0].viewport);

		// If more is than the static scene is rendered here, this stuff must go in the loop.
		commandList->setPipelineState(indirectShadowPipelineState);
		commandList->setGraphicsRootSignature(indirectShadowRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->setVertexBuffer(0, indirectMesh.vertexBuffer);
		commandList->setIndexBuffer(indirectMesh.indexBuffer);

		for (uint32 i = 0; i < sun.numShadowCascades; ++i)
		{
			commandList->setRenderTarget(sunShadowMapRT[i]);
			commandList->clearDepth(sunShadowMapRT[i].depthStencilAttachment->getDepthStencilView(), 1.f);

			// This only works, because the vertex shader expects the vp matrix as the first argument.
			commandList->setGraphics32BitConstants(INDIRECT_ROOTPARAM_CAMERA, sun.vp[i]);

			// Static scene.
			commandList->getD3D12CommandList()->ExecuteIndirect(
				indirectShadowCommandSignature.Get(),
				(uint32)indirectSubmeshes.size(),
				indirectShadowCommandBuffer.resource.Get(),
				0,
				nullptr,
				0);
		}
	}



	commandList->setViewport(viewport);

	// Render to GBuffer.
	{
		PROFILE_BLOCK("Record GBuffer commands");

		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "GBuffer.");

		commandList->setRenderTarget(gbufferRT);
		// No need to clear color, since we mark valid pixels with the stencil.
		commandList->clearDepthAndStencil(gbufferRT.depthStencilAttachment->getDepthStencilView());
		commandList->setStencilReference(1);

		// Static scene.
		{
			commandList->setPipelineState(indirectGeometryPipelineState);
			commandList->setGraphicsRootSignature(indirectGeometryRootSignature);

			commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandList->setGraphicsDynamicConstantBuffer(INDIRECT_ROOTPARAM_CAMERA, cameraCBAddress);

			commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, indirectDescriptorHeap);
			commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_ALBEDOS, albedosOffset);
			commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_NORMALS, normalsOffset);
			commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_ROUGHNESSES, roughnessesOffset);
			commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(INDIRECT_ROOTPARAM_METALLICS, metallicsOffset);
			commandList->setVertexBuffer(0, indirectMesh.vertexBuffer);
			commandList->setIndexBuffer(indirectMesh.indexBuffer);

			commandList->getD3D12CommandList()->ExecuteIndirect(
				indirectGeometryCommandSignature.Get(),
				(uint32)indirectSubmeshes.size(),
				indirectCommandBuffer.resource.Get(),
				0,
				nullptr,
				0);
		}

#if 0
		// Geometry.
		{
			commandList->setPipelineState(opaqueGeometryPipelineState);
			commandList->setGraphicsRootSignature(opaqueGeometryRootSignature);

			// This sets the adjacency information (list, strip, strip with adjacency, ...), 
			// while the pipeline state stores the input assembly type (points, lines, triangles, patches).
			commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandList->setGraphicsDynamicConstantBuffer(GEOMETRY_ROOTPARAM_CAMERA, cameraCBAddress);
			commandList->setGraphics32BitConstants(GEOMETRY_ROOTPARAM_MODEL, modelMatrix);

			commandList->setShaderResourceView(GEOMETRY_ROOTPARAM_TEXTURES, 0, cerberusMaterial.albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->setShaderResourceView(GEOMETRY_ROOTPARAM_TEXTURES, 1, cerberusMaterial.normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->setShaderResourceView(GEOMETRY_ROOTPARAM_TEXTURES, 2, cerberusMaterial.roughMetal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			commandList->setVertexBuffer(0, sceneMesh.vertexBuffer);
			commandList->setIndexBuffer(sceneMesh.indexBuffer);

			for (uint32 i = 0; i < sceneSubmeshes.size(); ++i)
			{
				submesh_info submesh = sceneSubmeshes[i];
				commandList->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
			}
		}
#endif
	}

	// Accumulate lighting.
	commandList->setRenderTarget(lightingRT);

	// Sky.
	sky.render(commandList, camera, cubemap);

	commandList->copyResource(depthTextureCopy, depthTexture);
	commandList->transitionBarrier(depthTexture, D3D12_RESOURCE_STATE_DEPTH_READ);

	// Lighting.
	lighting.render(commandList,
		irradiance, prefilteredEnvironment,
		albedoAOTexture, normalRoughnessMetalnessTexture, depthTextureCopy,
		sunShadowMapTexture, sun.numShadowCascades,
		cameraCBAddress, sunCBAddress);


	// Resolve to screen.
	// No need to clear RTV (or for a depth buffer), since we are blitting the whole lighting buffer.
	commandList->setScreenRenderTarget(&screenRTV, 1, nullptr);

	// Present.
	present.render(commandList, hdrTexture);

	// GUI.
	processAndDisplayProfileEvents(gui);
	gui.render(commandList, viewport); // Probably not completely correct here, since alpha blending assumes linear colors?
}

bool dx_game::keyDownCallback(keyboard_event event)
{
	switch (event.key)
	{
	case key_w: inputMovement.z -= 1.f; break;
	case key_s: inputMovement.z += 1.f; break;
	case key_a: inputMovement.x -= 1.f; break;
	case key_d: inputMovement.x += 1.f; break;
	case key_q: inputMovement.y -= 1.f; break;
	case key_e: inputMovement.y += 1.f; break;
	case key_shift: inputSpeedModifier = 3.f; break;
	}
	return true;
}

bool dx_game::keyUpCallback(keyboard_event event)
{
	switch (event.key)
	{
	case key_w: inputMovement.z += 1.f; break;
	case key_s: inputMovement.z -= 1.f; break;
	case key_a: inputMovement.x += 1.f; break;
	case key_d: inputMovement.x -= 1.f; break;
	case key_q: inputMovement.y += 1.f; break;
	case key_e: inputMovement.y -= 1.f; break;
	case key_shift: inputSpeedModifier = 1.f; break;
	}
	return true;
}

bool dx_game::mouseMoveCallback(mouse_move_event event)
{
	if (event.leftDown)
	{
		camera.pitch = camera.pitch - event.relDY * CAMERA_SENSITIVITY;
		camera.yaw = camera.yaw - event.relDX * CAMERA_SENSITIVITY;
	}
	return true;
}


