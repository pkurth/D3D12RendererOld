#include "pch.h"
#include "procedural_placement_editor.h"
#include "command_queue.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

struct brush_cb
{
	vec3 brushPosition;
	float brushRadius;
	float brushHardness;
	float brushStrength;
};

void procedural_placement_editor::initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget)
{
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/placement_editor_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/placement_editor_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_MODEL].InitAsConstants(sizeof(mat4) * 2 / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_CB].InitAsConstants(sizeof(brush_cb) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_TEX].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearWrapSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		visualizeDensityRootSignature.initialize(device, rootSignatureDesc);


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
		} pipelineStateStream;

		pipelineStateStream.rootSignature = visualizeDensityRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
		pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&visualizeDensityPipelineState)));

		SET_NAME(visualizeDensityRootSignature.rootSignature, "Placement Editor Visualize Density Root Signature");
		SET_NAME(visualizeDensityPipelineState, "Placement Editor Visualize Density Pipeline");
	}

	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/placement_editor_apply_brush_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/placement_editor_apply_brush_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_MODEL].InitAsConstants(sizeof(mat4) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_CB].InitAsConstants(sizeof(brush_cb) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		applyBrushRootSignature.initialize(device, rootSignatureDesc);

		D3D12_RT_FORMAT_ARRAY applyBrushRTFormat = {};
		applyBrushRTFormat.NumRenderTargets = 1;
		applyBrushRTFormat.RTFormats[0] = DXGI_FORMAT_R8_UNORM;

		struct pipeline_state_stream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS vs;
			CD3DX12_PIPELINE_STATE_STREAM_PS ps;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
		} pipelineStateStream;

		pipelineStateStream.rootSignature = applyBrushRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.rtvFormats = applyBrushRTFormat;
		pipelineStateStream.rasterizer = noBackfaceCullRasterizerDesc;

		CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
		depthDesc.DepthEnable = false;
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
		pipelineStateStream.depthStencilDesc = depthDesc;

		CD3DX12_BLEND_DESC blendDescs[] =
		{
			additiveBlendDesc,
			reverseSubtractiveBlendDesc,
			minBlendDesc,
			maxBlendDesc,
		};

		static_assert(arraysize(blendDescs) == placement_brush_count);

		for (uint32 i = 0; i < placement_brush_count; ++i)
		{
			pipelineStateStream.blend = blendDescs[i];

			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(pipeline_state_stream), &pipelineStateStream
			};
			checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&applyBrushPipelineState[i])));

			SET_NAME(applyBrushPipelineState[i], "Placement Editor Apply Brush Pipeline");
		}

		SET_NAME(applyBrushRootSignature.rootSignature, "Placement Editor Apply Brush Root Signature");
	}


	mouseDown = false;

	registerMouseButtonDownCallback(BIND(mouseDownCallback));
	registerMouseButtonUpCallback(BIND(mouseUpCallback));
	registerMouseMoveCallback(BIND(mouseMoveCallback));
}

void procedural_placement_editor::update(dx_command_list* commandList, const render_camera& camera, procedural_placement& placement, debug_gui& gui)
{
#if PROCEDURAL_PLACEMENT_ALLOW_SIMULTANEOUS_EDITING

	PROFILE_FUNCTION();

	
	DEBUG_TAB(gui, "Procedural placement")
	{
		const char* densities[] = { "1", "2", "3", "4" };

		DEBUG_GROUP(gui, "Brush")
		{
			gui.radio("Brush type", placementBrushNames, placement_brush_count, (uint32&)brushType);
			gui.slider("Brush radius", brushRadius, 0.1f, 50.f);
			gui.slider("Brush hardness", brushHardness, 0.f, 1.f);
			gui.slider("Brush strength", brushStrength, 0.f, 1.f);
			gui.radio("Density map index", densities, 4, densityMapIndex);
		}
		gui.textF("Mouse position: %.3f, %.3f", mousePosition.x, mousePosition.y);

		ray r = camera.getWorldSpaceRay(mousePosition.x, mousePosition.y);
		camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

		vec3 hitPosition(9999.f, 9999.f, 9999.f);
		bool hit = false;

		for (placement_tile& tile : placement.tiles)
		{
			vec2 corner0(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);
			vec2 corner1 = corner0 + vec2(PROCEDURAL_TILE_SIZE, PROCEDURAL_TILE_SIZE);

			vec4 plane(0.f, 1.f, 0.f, -tile.groundHeight);

			float t;
			if (r.intersectPlane(plane, t))
			{
				vec3 p = r.origin + t * r.direction;
				if (p.x >= corner0.x && p.x <= corner1.x &&
					p.y >= corner0.y && p.y <= corner1.y)
				{
					hit = true;
					hitPosition = p;
				}
			}
		}



		brush_cb brushCB;
		brushCB.brushPosition = hitPosition;
		brushCB.brushRadius = brushRadius;
		brushCB.brushHardness = (1.f - brushHardness) * 5.f;
		brushCB.brushStrength = brushStrength;


		vertex_3PU vertices[] =
		{
			{ vec3(0.f, 0.f, 0.f) * PROCEDURAL_TILE_SIZE, vec2(0.f, 0.f) },
			{ vec3(0.f, 0.f, 1.f) * PROCEDURAL_TILE_SIZE, vec2(0.f, 1.f) },
			{ vec3(1.f, 0.f, 0.f) * PROCEDURAL_TILE_SIZE, vec2(1.f, 0.f) },
			{ vec3(1.f, 0.f, 1.f) * PROCEDURAL_TILE_SIZE, vec2(1.f, 1.f) },
		};

		D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(vertices, arraysize(vertices));

		// Apply brush.
		if (mouseDown)
		{
			commandList->setPipelineState(applyBrushPipelineState[brushType]);
			commandList->setGraphicsRootSignature(applyBrushRootSignature);

			commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			commandList->setVertexBuffer(0, tmpVertexBuffer);
			commandList->setGraphics32BitConstants(PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_CB, brushCB);

			dx_render_target currentRT = *commandList->getCurrentRenderTarget();

			for (placement_tile& tile : placement.tiles)
			{
				if (tile.densities[densityMapIndex])
				{
					bounding_box tileBB = tile.aabb;

					if (tile.aabb.intersectSphere(hitPosition, brushRadius))
					{
						densityRT.attachColorTexture(0, *tile.densities[densityMapIndex]);
						commandList->setRenderTarget(densityRT);
						commandList->setViewport(densityRT.viewport);

						vec2 corner(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);

						mat4 m = createTranslationMatrix(corner.x, tile.groundHeight, corner.y);

						struct
						{
							mat4 m;
						} modelCB =
						{
							m,
						};

						commandList->setGraphics32BitConstants(PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_MODEL, modelCB);
						commandList->draw(4, 1, 0, 0);
					}
				}
			}

			commandList->setRenderTarget(currentRT);
			commandList->setViewport(currentRT.viewport);
		}

		// Render tiles.
		{
			PROFILE_BLOCK("Render tiles");

			commandList->setPipelineState(visualizeDensityPipelineState);
			commandList->setGraphicsRootSignature(visualizeDensityRootSignature);

			commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			commandList->setVertexBuffer(0, tmpVertexBuffer);
			commandList->setGraphics32BitConstants(PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_CB, brushCB);


			for (placement_tile& tile : placement.tiles)
			{
				if (tile.densities[densityMapIndex])
				{
					vec2 corner(tile.cornerX * PROCEDURAL_TILE_SIZE, tile.cornerZ * PROCEDURAL_TILE_SIZE);

					if (!frustum.cullWorldSpaceAABB(tile.aabb))
					{
						mat4 m = createTranslationMatrix(corner.x, tile.groundHeight, corner.y);

						struct
						{
							mat4 m;
							mat4 mvp;
						} modelCB =
						{
							m,
							camera.viewProjectionMatrix * m
						};

						commandList->setGraphics32BitConstants(PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_MODEL, modelCB);
						commandList->setShaderResourceView(PROCEDURAL_PLACEMENT_EDITOR_ROOTPARAM_TEX, 0, *tile.densities[densityMapIndex], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
						commandList->draw(4, 1, 0, 0);
					}
				}

			}
		}


		for (dx_texture* texture : placement.distinctDensityTextures)
		{
			commandList->transitionBarrier(*texture, D3D12_RESOURCE_STATE_COMMON);
		}
	}

#endif
}

bool procedural_placement_editor::mouseDownCallback(mouse_button_event event)
{
	mousePosition = vec2(event.relX, event.relY);
	if (event.button == mouse_left)
	{
		mouseDown = true;
	}
	return false;
}

bool procedural_placement_editor::mouseUpCallback(mouse_button_event event)
{
	mousePosition = vec2(event.relX, event.relY);
	if (event.button == mouse_left)
	{
		mouseDown = false;
	}
	return false;
}

bool procedural_placement_editor::mouseMoveCallback(mouse_move_event event)
{
	mousePosition = vec2(event.relX, event.relY);
	return false;
}
