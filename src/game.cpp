#include "pch.h"
#include "game.h"
#include "error.h"
#include "model.h"
#include "graphics.h"

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

	D3D12_RT_FORMAT_ARRAY screenRTVFormats = {};
	screenRTVFormats.NumRenderTargets = 1;
	if (colorDepth == color_depth_8)
	{
		screenRTVFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else
	{
		assert(colorDepth == color_depth_10);
		screenRTVFormats.RTFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
	}

	// GBuffer.
	{
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
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |  // Allows creation of depth-stencil-view (so that we can write to it).
				D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

			D3D12_CLEAR_VALUE depthClearValue;
			depthClearValue.Format = depthDesc.Format;
			depthClearValue.DepthStencil = { 1.f, 0 };

			depthTexture.initialize(device, depthDesc, &depthClearValue);

			gbufferRT.attachDepthStencilTexture(depthTexture);
			lightingRT.attachDepthStencilTexture(depthTexture);
		}

		// Sun shadow map.
		{
			DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
			CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, 2048, 2048);
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			depthDesc.MipLevels = 1;

			D3D12_CLEAR_VALUE depthClearValue;
			depthClearValue.Format = depthBufferFormat;
			depthClearValue.DepthStencil = { 1.f, 0 };

			sunShadowMapTexture.initialize(device, depthDesc, &depthClearValue);

			sunShadowMapRT.attachDepthStencilTexture(sunShadowMapTexture);
		}
	}

	// AZDO.
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/azdo_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/azdo_ps.cso", &pixelShaderBlob));

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
		rootParameters[AZDO_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Camera.
		rootParameters[AZDO_ROOTPARAM_MODEL].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // Model matrix (mat4).
		rootParameters[AZDO_ROOTPARAM_MATERIAL].InitAsConstants(sizeof(material_cb) / sizeof(float), 2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Material.
		rootParameters[AZDO_ROOTPARAM_ALBEDOS].InitAsDescriptorTable(1, &albedos, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[AZDO_ROOTPARAM_NORMALS].InitAsDescriptorTable(1, &normals, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[AZDO_ROOTPARAM_ROUGHNESSES].InitAsDescriptorTable(1, &roughnesses, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[AZDO_ROOTPARAM_METALLICS].InitAsDescriptorTable(1, &metallics, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearWrapSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		azdoGeometryRootSignature.initialize(device, rootSignatureDesc);

		
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

			pipelineStateStream.rootSignature = azdoGeometryRootSignature.rootSignature.Get();
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
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&azdoGeometryPipelineState)));
		}

		// Shadow pass.
		rootSignatureDesc.NumParameters = 2; // Don't need the materials.
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		azdoShadowRootSignature.initialize(device, rootSignatureDesc);

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

			pipelineStateStream.rootSignature = azdoShadowRootSignature.rootSignature.Get();
			pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
			pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.dsvFormat = sunShadowMapRT.depthStencilFormat;
			pipelineStateStream.rasterizer = defaultRasterizerDesc;

			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(pipeline_state_stream), &pipelineStateStream
			};
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&azdoShadowPipelineState)));
		}


		SET_NAME(azdoGeometryRootSignature.rootSignature, "AZDO Root Signature");
		SET_NAME(azdoGeometryPipelineState, "AZDO Pipeline");
		SET_NAME(azdoShadowRootSignature.rootSignature, "AZDO Shadow Root Signature");
		SET_NAME(azdoShadowPipelineState, "AZDO Shadow Pipeline");


		// Command Signature.

		D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[3];
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[0].Constant.RootParameterIndex = AZDO_ROOTPARAM_MODEL;
		argumentDescs[0].Constant.DestOffsetIn32BitValues = 0;
		argumentDescs[0].Constant.Num32BitValuesToSet = 16;

		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[1].Constant.RootParameterIndex = AZDO_ROOTPARAM_MATERIAL;
		argumentDescs[1].Constant.DestOffsetIn32BitValues = 0;
		argumentDescs[1].Constant.Num32BitValuesToSet = rootParameters[AZDO_ROOTPARAM_MATERIAL].Constants.Num32BitValues;

		argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
		commandSignatureDesc.pArgumentDescs = argumentDescs;
		commandSignatureDesc.NumArgumentDescs = arraysize(argumentDescs);
		commandSignatureDesc.ByteStride = sizeof(indirect_command);

		checkResult(device->CreateCommandSignature(&commandSignatureDesc, azdoGeometryRootSignature.rootSignature.Get(),
			IID_PPV_ARGS(&azdoGeometryCommandSignature)));
		SET_NAME(azdoGeometryCommandSignature, "AZDO Command Signature");


		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		commandSignatureDesc.NumArgumentDescs = 2;
		commandSignatureDesc.ByteStride = sizeof(indirect_shadow_command);

		checkResult(device->CreateCommandSignature(&commandSignatureDesc, azdoShadowRootSignature.rootSignature.Get(),
			IID_PPV_ARGS(&azdoShadowCommandSignature)));
		SET_NAME(azdoShadowCommandSignature, "AZDO Shadow Command Signature");
	}

	// Geometry. This writes to the GBuffer.
	{
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

	// Sky. This writes to the lighting RT.
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/sky_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/sky_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[SKY_ROOTPARAM_VP].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // 1 matrix.
		rootParameters[SKY_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		skyRootSignature.initialize(device, rootSignatureDesc);

		
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

		pipelineStateStream.rootSignature = skyRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = lightingRT.depthStencilFormat;
		pipelineStateStream.rtvFormats = lightingRT.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;
		pipelineStateStream.depthStencilDesc = notEqualStencilDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&skyPipelineState)));

		SET_NAME(skyRootSignature.rootSignature, "Sky Root Signature");
		SET_NAME(skyPipelineState, "Sky Pipeline");
	}

	ComPtr<ID3DBlob> fullscreenTriangleVertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/fullscreen_triangle_vs.cso", &fullscreenTriangleVertexShaderBlob));


	// Lighting.
	{
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/lighting_ps.cso", &pixelShaderBlob));

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[LIGHTING_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Camera.
		rootParameters[LIGHTING_ROOTPARAM_TEXTURES].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[LIGHTING_ROOTPARAM_DIRECTIONAL].InitAsConstants(sizeof(directional_light) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		lightingRootSignature.initialize(device, rootSignatureDesc);

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
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = lightingRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { nullptr, 0 };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(fullscreenTriangleVertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.rtvFormats = lightingRT.renderTargetFormat;
		pipelineStateStream.dsvFormat = lightingRT.depthStencilFormat;

		CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc(D3D12_DEFAULT);
		depthStencilDesc.DepthEnable = false; // Don't do depth-check.
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL; // Write where geometry is.
		depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP; // Don't modify stencil.
		pipelineStateStream.depthStencilDesc = depthStencilDesc;

		pipelineStateStream.blend = additiveBlendDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&lightingPipelineState)));

		SET_NAME(lightingRootSignature.rootSignature, "Lighting Root Signature");
		SET_NAME(lightingPipelineState, "Lighting Pipeline");
	}

	// Present.
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/fullscreen_triangle_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/present_ps.cso", &pixelShaderBlob));

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[4];
		rootParameters[PRESENT_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Camera.
		rootParameters[PRESENT_ROOTPARAM_MODE].InitAsConstants(2, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Present params.
		rootParameters[PRESENT_ROOTPARAM_TONEMAP].InitAsConstants(8, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Tonemap params.
		rootParameters[PRESENT_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL); // Texture.

		CD3DX12_STATIC_SAMPLER_DESC sampler(0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		presentRootSignature.initialize(device, rootSignatureDesc);


		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS vs;
			CD3DX12_PIPELINE_STATE_STREAM_PS ps;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = presentRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { nullptr, 0 };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.rtvFormats = screenRTVFormats;

		CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
		depthDesc.DepthEnable = false; // Don't do depth-check.
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
		pipelineStateStream.depthStencilDesc = depthDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&presentPipelineState)));

		SET_NAME(presentRootSignature.rootSignature, "Present Root Signature");
		SET_NAME(presentPipelineState, "Present Pipeline");
	}


	dx_command_queue& copyCommandQueue = dx_command_queue::copyCommandQueue;
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();

	gui.initialize(device, commandList, screenRTVFormats);


	// Load scene.
	/*cpu_mesh<vertex_3PUNT> scene;
	append(sceneSubmeshes, scene.pushFromFile("res/cerberus/Cerberus_LP.FBX").first);
	
	sceneMesh.initialize(device, commandList, scene);

	commandList->loadTextureFromFile(cerberusMaterial.albedo, L"res/cerberus/Cerberus_A.tga", texture_type_color);
	commandList->loadTextureFromFile(cerberusMaterial.normal, L"res/cerberus/Cerberus_N.tga", texture_type_noncolor);
	commandList->loadTextureFromFile(cerberusMaterial.roughMetal, L"res/cerberus/Cerberus_RMAO.png", texture_type_noncolor);*/


	cpu_mesh<vertex_3P> skybox;
	skySubmesh = skybox.pushCube(1.f, true);
	skyMesh.initialize(device, commandList, skybox);


	cpu_mesh<vertex_3PUNT> azdo;
	auto[sponzaSubmeshes, sponzaMaterials] = azdo.pushFromFile("res/sponza/sponza.obj");
	append(azdoSubmeshes, sponzaSubmeshes);

	azdoMesh.initialize(device, commandList, azdo);

	azdoMaterials.resize(sponzaMaterials.size());
	for (uint32 i = 0; i < sponzaMaterials.size(); ++i)
	{
		commandList->loadTextureFromFile(azdoMaterials[i].albedo, stringToWString(sponzaMaterials[i].albedoName), texture_type_color);
		commandList->loadTextureFromFile(azdoMaterials[i].normal, stringToWString(sponzaMaterials[i].normalName), texture_type_noncolor);
		commandList->loadTextureFromFile(azdoMaterials[i].roughness, stringToWString(sponzaMaterials[i].roughnessName), texture_type_noncolor);
		commandList->loadTextureFromFile(azdoMaterials[i].metallic, stringToWString(sponzaMaterials[i].metallicName), texture_type_noncolor);
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NumDescriptors = (uint32)azdoMaterials.size() * 4;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	checkResult(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&azdoDescriptorHeap)));

	uint32 descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(azdoDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(azdoDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	albedosOffset = gpuHandle;
	for (uint32 i = 0; i < azdoMaterials.size(); ++i)
	{
		device->CreateShaderResourceView(azdoMaterials[i].albedo.resource.Get(), nullptr, cpuHandle);
		cpuHandle.Offset(descriptorHandleIncrementSize);
		gpuHandle.Offset(descriptorHandleIncrementSize);
	}
	normalsOffset = gpuHandle;
	for (uint32 i = 0; i < azdoMaterials.size(); ++i)
	{
		device->CreateShaderResourceView(azdoMaterials[i].normal.resource.Get(), nullptr, cpuHandle);
		cpuHandle.Offset(descriptorHandleIncrementSize);
		gpuHandle.Offset(descriptorHandleIncrementSize);
	}
	roughnessesOffset = gpuHandle;
	for (uint32 i = 0; i < azdoMaterials.size(); ++i)
	{
		device->CreateShaderResourceView(azdoMaterials[i].roughness.resource.Get(), nullptr, cpuHandle);
		cpuHandle.Offset(descriptorHandleIncrementSize);
		gpuHandle.Offset(descriptorHandleIncrementSize);
	}
	metallicsOffset = gpuHandle;
	for (uint32 i = 0; i < azdoMaterials.size(); ++i)
	{
		device->CreateShaderResourceView(azdoMaterials[i].metallic.resource.Get(), nullptr, cpuHandle);
		cpuHandle.Offset(descriptorHandleIncrementSize);
		gpuHandle.Offset(descriptorHandleIncrementSize);
	}

	indirect_command* azdoCommands = new indirect_command[sponzaSubmeshes.size()];
	indirect_shadow_command* azdoShadowCommands = new indirect_shadow_command[sponzaSubmeshes.size()];

	mat4 model = createScaleMatrix(0.03f);

	for (uint32 i = 0; i < sponzaSubmeshes.size(); ++i)
	{
		azdoCommands[i].modelMatrix = model;
		azdoShadowCommands[i].modelMatrix = model;

		uint32 id = i;

		submesh_info mesh = azdoSubmeshes[id];

		azdoCommands[i].material.textureID = mesh.materialIndex;
		azdoCommands[i].material.usageFlags = (USE_ALBEDO_TEXTURE | USE_NORMAL_TEXTURE | USE_ROUGHNESS_TEXTURE | USE_METALLIC_TEXTURE | USE_AO_TEXTURE);
		azdoCommands[i].material.albedoTint = vec4(1.f, 1.f, 1.f, 1.f);
		azdoCommands[i].material.drawID = i;

		azdoCommands[i].drawArguments.IndexCountPerInstance = mesh.numTriangles * 3;
		azdoCommands[i].drawArguments.InstanceCount = 1;
		azdoCommands[i].drawArguments.StartIndexLocation = mesh.firstTriangle * 3;
		azdoCommands[i].drawArguments.BaseVertexLocation = mesh.baseVertex;
		azdoCommands[i].drawArguments.StartInstanceLocation = 0;

		azdoShadowCommands[i].drawArguments = azdoCommands[i].drawArguments;
	}

	azdoCommandBuffer.initialize(device, azdoCommands, (uint32)sponzaSubmeshes.size(), commandList);
	azdoShadowCommandBuffer.initialize(device, azdoShadowCommands, (uint32)sponzaSubmeshes.size(), commandList);
	
	delete[] azdoShadowCommands;
	delete[] azdoCommands;

	dx_texture equirectangular;
	commandList->loadTextureFromFile(equirectangular, L"res/leadenhall_market_4k.hdr", texture_type_color);
	commandList->convertEquirectangularToCubemap(equirectangular, cubemap, 1024, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
	commandList->createIrradianceMap(cubemap, irradiance);
	commandList->prefilterEnvironmentMap(cubemap, prefilteredEnvironment, 256);
	commandList->integrateBRDF(brdf);


	uint64 fenceValue = copyCommandQueue.executeCommandList(commandList);
	copyCommandQueue.waitForFenceValue(fenceValue);

	dx_command_queue& renderCommandQueue = dx_command_queue::renderCommandQueue;
	commandList = renderCommandQueue.getAvailableCommandList();


	// Transition AZDO textures to pixel shader state.
	for (uint32 i = 0; i < azdoMaterials.size(); ++i)
	{
		commandList->transitionBarrier(azdoMaterials[i].albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->transitionBarrier(azdoMaterials[i].normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->transitionBarrier(azdoMaterials[i].roughness, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->transitionBarrier(azdoMaterials[i].metallic, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	fenceValue = renderCommandQueue.executeCommandList(commandList);
	renderCommandQueue.waitForFenceValue(fenceValue);

	
	// Loading scene done.
	contentLoaded = true;

	this->width = width;
	this->height = height;
	viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);
	flushApplication();

	camera.fovY = DirectX::XMConvertToRadians(70.f);
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
	}
}

void dx_game::update(float dt)
{
	camera.rotation = createQuaternionFromAxisAngle(comp_vec(0.f, 1.f, 0.f), camera.yaw) 
		* createQuaternionFromAxisAngle(comp_vec(1.f, 0.f, 0.f), camera.pitch);

	camera.position = camera.position + camera.rotation * inputMovement * dt * CAMERA_MOVEMENT_SPEED * inputSpeedModifier;
	camera.updateMatrices(width, height);

	this->dt = dt;

	DEBUG_TAB(gui, "Stats")
	{
		gui.textF("Performance: %.2f fps (%.3f ms)", 1.f / dt, dt * 1000.f);
		DEBUG_GROUP(gui, "Camera")
		{
			gui.textF("Camera position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
			gui.textF("Input movement: %.2f, %.2f, %.2f", inputMovement.x, inputMovement.y, inputMovement.z);
		}
		gui.textF("%u draw calls", (uint32)azdoSubmeshes.size());
	}
}

void dx_game::render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV)
{
	// Currently not needed, since we don't use a depth buffer for the screen.
	//D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();

	PIXSetMarker(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 0, 0), "Frame start.");

	camera_cb cameraCB;
	camera.fillConstantBuffer(cameraCB);

	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress = commandList->uploadDynamicConstantBuffer(cameraCB);

	commandList->setScissor(scissorRect);


	// Render to sun shadow map.

	commandList->setViewport(sunShadowMapRT.viewport);

	PIXBeginEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Sun shadow map.");

	commandList->setRenderTarget(sunShadowMapRT);
	commandList->clearDepth(sunShadowMapRT.depthStencilAttachment->getDepthStencilView(), 1.f);

#if 1
	// Static scene.
	{
		commandList->setPipelineState(azdoShadowPipelineState);
		commandList->setGraphicsRootSignature(azdoShadowRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->setGraphicsDynamicConstantBuffer(AZDO_ROOTPARAM_CAMERA, cameraCBAddress);

		commandList->setVertexBuffer(0, azdoMesh.vertexBuffer);
		commandList->setIndexBuffer(azdoMesh.indexBuffer);

		commandList->getD3D12CommandList()->ExecuteIndirect(
			azdoShadowCommandSignature.Get(),
			(uint32)azdoSubmeshes.size(),
			azdoShadowCommandBuffer.resource.Get(),
			0,
			nullptr,
			0);
	}
#endif


	PIXEndEvent(commandList->getD3D12CommandList().Get());


	commandList->setViewport(viewport);


	PIXBeginEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "GBuffer.");

	// Render to GBuffer.
	commandList->setRenderTarget(gbufferRT);
	// No need to clear color, since we mark valid pixels with the stencil.
	commandList->clearDepthAndStencil(gbufferRT.depthStencilAttachment->getDepthStencilView());
	commandList->setStencilReference(1);

	// Static scene.
	{
		commandList->setPipelineState(azdoGeometryPipelineState);
		commandList->setGraphicsRootSignature(azdoGeometryRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->setGraphicsDynamicConstantBuffer(AZDO_ROOTPARAM_CAMERA, cameraCBAddress);

		commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, azdoDescriptorHeap);
		commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(AZDO_ROOTPARAM_ALBEDOS, albedosOffset);
		commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(AZDO_ROOTPARAM_NORMALS, normalsOffset);
		commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(AZDO_ROOTPARAM_ROUGHNESSES, roughnessesOffset);
		commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(AZDO_ROOTPARAM_METALLICS, metallicsOffset);
		commandList->setVertexBuffer(0, azdoMesh.vertexBuffer);
		commandList->setIndexBuffer(azdoMesh.indexBuffer);
		
		commandList->getD3D12CommandList()->ExecuteIndirect(
			azdoGeometryCommandSignature.Get(),
			(uint32)azdoSubmeshes.size(),
			azdoCommandBuffer.resource.Get(),
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

	PIXEndEvent(commandList->getD3D12CommandList().Get());

	PIXBeginEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Sky.");


	// Accumulate lighting.
	commandList->setRenderTarget(lightingRT);

	// Sky.
	{
		commandList->setPipelineState(skyPipelineState);
		commandList->setGraphicsRootSignature(skyRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		mat4 view = camera.viewMatrix;
		view.m03 = 0.f; view.m13 = 0.f; view.m23 = 0.f;
		mat4 skyVP = camera.projectionMatrix * view;

		commandList->setGraphics32BitConstants(SKY_ROOTPARAM_VP, skyVP);

		commandList->setVertexBuffer(0, skyMesh.vertexBuffer);
		commandList->setIndexBuffer(skyMesh.indexBuffer);
		commandList->bindCubemap(SKY_ROOTPARAM_TEXTURE, 0, cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		commandList->drawIndexed(skyMesh.indexBuffer.numIndices, 1, 0, 0, 0);
	}

	PIXEndEvent(commandList->getD3D12CommandList().Get());

	PIXBeginEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Lighting pass.");

	// Lighting.
	{
		directional_light sun = { comp_vec(-1.f, -1.f, -1.f, 0.f).normalize(), vec4(3.f, 0.f, 0.f, 1.f) };

		commandList->setPipelineState(lightingPipelineState);
		commandList->setGraphicsRootSignature(lightingRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->setGraphicsDynamicConstantBuffer(LIGHTING_ROOTPARAM_CAMERA, cameraCBAddress);

		commandList->bindCubemap(LIGHTING_ROOTPARAM_TEXTURES, 0, irradiance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->bindCubemap(LIGHTING_ROOTPARAM_TEXTURES, 1, prefilteredEnvironment, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->setShaderResourceView(LIGHTING_ROOTPARAM_TEXTURES, 2, brdf, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->setShaderResourceView(LIGHTING_ROOTPARAM_TEXTURES, 3, albedoAOTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->setShaderResourceView(LIGHTING_ROOTPARAM_TEXTURES, 4, normalRoughnessMetalnessTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		commandList->setGraphics32BitConstants(LIGHTING_ROOTPARAM_DIRECTIONAL, sun);

		commandList->draw(3, 1, 0, 0);
	}

	PIXEndEvent(commandList->getD3D12CommandList().Get());

	// Resolve to screen.
	// No need to clear RTV (or for a depth buffer), since we are blitting the whole lighting buffer.
	commandList->setScreenRenderTarget(&screenRTV, 1, nullptr);

	// Present.
	{
		commandList->setPipelineState(presentPipelineState);
		commandList->setGraphicsRootSignature(presentRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		struct present_cb
		{
			uint32 displayMode;
			float standardNits;
		} presentCB =
		{
			0, 0.f
		};

		struct tonemap_cb
		{
			float A; // Shoulder strength.
			float B; // Linear strength.
			float C; // Linear angle.
			float D; // Toe strength.
			float E; // Toe Numerator.
			float F; // Toe denominator.
			// Note E/F = Toe angle.
			float linearWhite;
			float exposure; // 0 is default.
		};

		tonemap_cb tonemapCB;
		tonemapCB.exposure = 1.f;
		tonemapCB.A = 0.22f;
		tonemapCB.B = 0.3f;
		tonemapCB.C = 0.1f;
		tonemapCB.D = 0.2f;
		tonemapCB.E = 0.01f;
		tonemapCB.F = 0.3f;
		tonemapCB.linearWhite = 11.2f;

		commandList->setGraphics32BitConstants(PRESENT_ROOTPARAM_MODE, presentCB);
		commandList->setGraphics32BitConstants(PRESENT_ROOTPARAM_TONEMAP, tonemapCB);

		commandList->setShaderResourceView(PRESENT_ROOTPARAM_TEXTURE, 0, hdrTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		commandList->draw(3, 1, 0, 0);
	}

	// GUI.
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


