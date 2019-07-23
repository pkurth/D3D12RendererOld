#include "pch.h"
#include "game.h"
#include "commands.h"
#include "error.h"
#include "model.h"


void loadScene(ComPtr<ID3D12Device2> device, scene_data& result)
{
	cpu_mesh_group<vertex_3PUN> model;
	model.loadFromFile("res/big_oak.obj");

	dx_command_queue& copyCommandQueue = dx_command_queue::copyCommandQueue;
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();


	result.textures.resize(model.meshes.size());
	for (uint32 i = 0; i < model.meshes.size(); ++i)
	{
		cpu_mesh<vertex_3PUN>& mesh = model.meshes[i];
		result.meshes.push_back(commandList->createMesh(mesh));
		commandList->loadTextureFromFile(result.textures[i], mesh.material.albedo, texture_usage_albedo);
	}

	cpu_mesh<vertex_3P> skybox = cpu_mesh<vertex_3P>::cube(1.f, true);
	result.skyMesh = commandList->createMesh(skybox);

	cpu_mesh<vertex_3PUN> quad = cpu_mesh<vertex_3PUN>::quad();
	result.quad = commandList->createMesh(quad);

	commandList->loadTextureFromFile(result.equirectangular, L"res/pano.hdr", texture_usage_albedo);
	commandList->convertEquirectangularToCubemap(result.equirectangular, result.cubemap, 1024, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
	//commandList->createIrradianceMap(result.cubemap, result.irradiance);
	//commandList->prefilterEnvironmentMap(result.cubemap, result.prefilteredEnvironment);
	//commandList->integrateBRDF(result.brdf);

	uint64 fenceValue = copyCommandQueue.executeCommandList(commandList);
	copyCommandQueue.waitForFenceValue(fenceValue);
}

void dx_game::initialize(ComPtr<ID3D12Device2> device, uint32 width, uint32 height, color_depth colorDepth)
{
	this->device = device;
	scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	checkResult(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));

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
		// Albedo.
		{
			DXGI_FORMAT albedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Alpha is unused for now.
			CD3DX12_RESOURCE_DESC albedoTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(albedoFormat, width, height);
			albedoTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE albedoClearValue;
			albedoClearValue.Format = albedoTextureDesc.Format;
			albedoClearValue.Color[0] = 0.f;
			albedoClearValue.Color[1] = 0.f;
			albedoClearValue.Color[2] = 0.f;
			albedoClearValue.Color[3] = 0.f;

			dx_texture albedoTexture;
			albedoTexture.initialize(device, texture_usage_render_target, albedoTextureDesc, &albedoClearValue);
			gBufferRT.attachTexture(render_target_attachment_point_color0, albedoTexture);
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

			dx_texture hdrTexture;
			hdrTexture.initialize(device, texture_usage_render_target, hdrTextureDesc, &hdrClearValue);
			gBufferRT.attachTexture(render_target_attachment_point_color1, hdrTexture);
			lightingRT.attachTexture(render_target_attachment_point_color0, hdrTexture);
		}

		// Normals.
		{
			DXGI_FORMAT normalMetalnessRoughnessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
			CD3DX12_RESOURCE_DESC normalMetalnessRoughnessTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(normalMetalnessRoughnessFormat, width, height);
			normalMetalnessRoughnessTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE normalClearValue;
			normalClearValue.Format = normalMetalnessRoughnessTextureDesc.Format;
			normalClearValue.Color[0] = 0.f;
			normalClearValue.Color[1] = 0.f;
			normalClearValue.Color[2] = 0.f;
			normalClearValue.Color[3] = 1.f;

			dx_texture normalTexture;
			normalTexture.initialize(device, texture_usage_render_target, normalMetalnessRoughnessTextureDesc, &normalClearValue);
			gBufferRT.attachTexture(render_target_attachment_point_color2, normalTexture);
		}

		// Depth.
		{
			DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // We need stencil for deferred lighting.
			CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, width, height);
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // Allows creation of depth-stencil-view (so that we can write to it).

			D3D12_CLEAR_VALUE depthClearValue;
			depthClearValue.Format = depthDesc.Format;
			depthClearValue.DepthStencil = { 1.f, 0 };

			dx_texture depthTexture;
			depthTexture.initialize(device, texture_usage_render_target, depthDesc, &depthClearValue);

			gBufferRT.attachTexture(render_target_attachment_point_depthstencil, depthTexture);
			lightingRT.attachTexture(render_target_attachment_point_depthstencil, depthTexture);
		}
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
		};


		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsConstants(16 * 3, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // 4 * 16 floats (mat4).
		rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler(0,
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP);

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
		pipelineStateStream.dsvFormat = gBufferRT.depthStencilFormat;
		pipelineStateStream.rtvFormats = gBufferRT.renderTargetFormat;
		pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(
			D3D12_FILL_MODE_SOLID,
			D3D12_CULL_MODE_NONE, // Disable backface culling for the leaves.
			TRUE, // Righthanded coordinate system.
			D3D12_DEFAULT_DEPTH_BIAS,
			D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			TRUE,
			FALSE,
			FALSE,
			0,
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
		);

		CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc(D3D12_DEFAULT);
		depthStencilDesc.StencilEnable = true;
		// Set front and back faces, since both are rendered (backface culling is disabled).
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		pipelineStateStream.depthStencilDesc = depthStencilDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&opaqueGeometryPipelineState)));
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
		rootParameters[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // 1 matrix.
		rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler(0,
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

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
		pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(
			D3D12_FILL_MODE_SOLID,
			D3D12_CULL_MODE_BACK,
			TRUE,
			D3D12_DEFAULT_DEPTH_BIAS,
			D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			TRUE,
			FALSE,
			FALSE,
			0,
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
		);

		CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc(D3D12_DEFAULT);
		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
		pipelineStateStream.depthStencilDesc = depthStencilDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&skyPipelineState)));
	}

	// Directional Light
	{

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
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsConstants(2, 0);
		rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler(0,
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
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
	}


	loadScene(device, scene);

	contentLoaded = true;

	this->width = width;
	this->height = height;
	resizeDepthBuffer(width, height);

	camera.fov = DirectX::XMConvertToRadians(70.f);
	camera.position = vec3(0.f, 0.f, 5.f);
	camera.rotation = quat::Identity;
	camera.update(width, height, 0.f);
}

void dx_game::resize(uint32 width, uint32 height)
{
	if (width != this->width || height != this->height)
	{
		this->width = width;
		this->height = height;
		viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)width, (float)height);
		resizeDepthBuffer(width, height);
	}
}

void dx_game::resizeDepthBuffer(uint32 width, uint32 height)
{
	if (contentLoaded)
	{
		// Flush any GPU commands that might be referencing the depth buffer.
		flushApplication();

		width = max(1u, width);
		height = max(1u, height);

		// Resize screen dependent resources.
		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		checkResult(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
				1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&depthBuffer)
		));

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(depthBuffer.Get(), &dsv,
			dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
}

void dx_game::update(float dt)
{
	static float totalTime = 0.f;
	totalTime += dt;
	float angle = totalTime * 45.f;
	vec3 rotationAxis(0, 1, 0);
	modelMatrix = mat4::CreateFromAxisAngle(rotationAxis, DirectX::XMConvertToRadians(angle));

	camera.update(width, height, dt);
}

void dx_game::render(dx_command_list* commandList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtv)
{
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();


	commandList->setViewport(viewport);
	commandList->setScissor(scissorRect);


	// Render to GBuffer.
	commandList->setRenderTarget(gBufferRT);
	// No need to clear color, since we mark valid pixels with the stencil.
	commandList->clearDepthAndStencil(gBufferRT.attachments[render_target_attachment_point_depthstencil].getDepthStencilView());
	commandList->setStencilReference(1);

	// Geometry.
	{
		commandList->setPipelineState(opaqueGeometryPipelineState);
		commandList->setGraphicsRootSignature(opaqueGeometryRootSignature);

		// This sets the adjacency information (list, strip, strip with adjacency, ...), 
		// while the pipeline state stores the input assembly type (points, lines, triangles, patches).
		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		struct cb
		{
			mat4 modelMatrix;
			mat4 viewMatrix;
			mat4 projMatrix;
		} c = {
			modelMatrix, camera.viewMatrix, camera.projectionMatrix
		};

		commandList->setGraphics32BitConstants(0, c);

		for (uint32 i = 0; i < scene.meshes.size(); ++i)
		{
			commandList->setVertexBuffer(0, scene.meshes[i].vertexBuffer);
			commandList->setIndexBuffer(scene.meshes[i].indexBuffer);
			commandList->setShaderResourceView(1, 0, scene.textures[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			commandList->drawIndexed(scene.meshes[i].indexBuffer.numIndices, 1, 0, 0, 0);
		}
	}


	// Accumulate lighting.
	commandList->setRenderTarget(lightingRT);

	// Sky.
	{
		commandList->setPipelineState(skyPipelineState);
		commandList->setGraphicsRootSignature(skyRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		mat4 vp = camera.projectionMatrix;
		commandList->setGraphics32BitConstants(0, vp);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = scene.cubemap.resource->GetDesc().Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = (UINT)-1; // Use all mips.

		commandList->setVertexBuffer(0, scene.skyMesh.vertexBuffer);
		commandList->setIndexBuffer(scene.skyMesh.indexBuffer);
		commandList->setShaderResourceView(1, 0, scene.cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &srvDesc);

		commandList->drawIndexed(scene.skyMesh.indexBuffer.numIndices, 1, 0, 0, 0);
	}


	// Resolve to screen.
	// No need to clear RTV, since we are blitting the whole lighting buffer.
	commandList->setScreenRenderTarget(&rtv, 1, &dsv);

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

		commandList->setGraphics32BitConstants(0, presentCB);
		commandList->setShaderResourceView(1, 0, lightingRT.attachments[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		commandList->drawIndexed(3, 1, 0, 0, 0);
	}

}


