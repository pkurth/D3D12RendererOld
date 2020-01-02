#include "pch.h"
#include "game.h"
#include "error.h"
#include "model.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>

/*
	Rendering TODOs:
		- Anti aliasing.
		- Multiple command lists.
		- Async compute:
			- Frustum and occlusion culling.
			- Light culling.
			- GPU procedural placement.
			- SSAO.
			- Particles.
		- LOD.
		- Specular reflections.
		- Volumetrics?
		- VFX.
		- Raytracing.
*/

#define ENABLE_PARTICLES 0
#define ENABLE_PROCEDURAL 1
#define ENABLE_PROCEDURAL_SHADOWS 0

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

		// Color.
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
			lightingRT.attachColorTexture(0, hdrTexture);
		}

		// Depth.
		{
			DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
			CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, width, height);
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			depthDesc.MipLevels = 1;

			D3D12_CLEAR_VALUE depthClearValue;
			depthClearValue.Format = depthDesc.Format;
			depthClearValue.DepthStencil = { 1.f, 0 };

			depthTexture.initialize(device, depthDesc, &depthClearValue);
			lightingRT.attachDepthStencilTexture(depthTexture);

		}

		// Shadow maps.
		{
			// Sun.
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

			// Spot light.
			depthDesc.Width = depthDesc.Height = spotLight.shadowMapDimensions;
			spotLightShadowMapTexture.initialize(device, depthDesc, &depthClearValue);
			spotLightShadowMapRT.attachDepthStencilTexture(spotLightShadowMapTexture);
		}
	}


	dx_command_queue& copyCommandQueue = dx_command_queue::copyCommandQueue;
	dx_command_list* commandList = copyCommandQueue.getAvailableCommandList();


	indirect.initialize(device, lightingRT, sunShadowMapRT->depthStencilFormat);
	proceduralPlacement.initialize(device, 1);
	commandList->loadTextureFromFile(densityMap, L"res/density.png", texture_type_noncolor, false);
	SET_NAME(densityMap.resource, "Density");


	{
		PROFILE_BLOCK("Sky, lighting, particle, present pipeline");
		sky.initialize(device, commandList, lightingRT);
		present.initialize(device, screenRTFormats);

		particles.initialize(device, lightingRT);

		commandList->integrateBRDF(brdf);
	}

#if ENABLE_PARTICLES
	{
		PROFILE_BLOCK("Particle system");
		
		particleSystem1.initialize(10000);
		particleSystem1.color.initializeAsLinear(vec4(0.7f, 0.3f, 0.4f, 1.f), vec4(0.8f, 0.8f, 0.1f, 1.f));
		particleSystem1.maxLifetime.initializeAsRandom(0.2f, 1.5f);
		particleSystem1.startVelocity.initializeAsRandom(vec3(-1.f, -1.f, -1.f), vec3(1.f, 1.f, 1.f));
		particleSystem1.spawnRate = 2000.f;
		particleSystem1.gravityFactor = 1.f;

		particleSystem2.initialize(10000);
		particleSystem2.color.initializeAsRandom(vec4(0.f, 0.f, 0.f, 0.f), vec4(1.f, 1.f, 1.f, 1.f));
		particleSystem2.maxLifetime.initializeAsConstant(1.f);
		particleSystem2.startVelocity.initializeAsRandom(vec3(-1.f, -1.f, -1.f), vec3(1.f, 1.f, 1.f));
		particleSystem2.spawnRate = 2000.f;
		particleSystem2.gravityFactor = 1.f;

		particleSystem3.initialize(10000);
		particleSystem3.spawnPosition = vec3(0.f, 3.f, 0.f);
		particleSystem3.color.initializeAsLinear(vec4(4.f, 3.f, 10.f, 0.02f), vec4(0.4f, 0.1f, 0.2f, 0.f));
		particleSystem3.maxLifetime.initializeAsRandom(1.f, 1.5f);
		particleSystem3.startVelocity.initializeAsRandom(vec3(-1.f, -1.f, -1.f), vec3(1.f, 1.f, 1.f));
		particleSystem3.spawnRate = 2000.f;
		commandList->loadTextureFromFile(particleSystem3.textureAtlas, L"res/fire_atlas.png", texture_type_color);
		particleSystem3.textureAtlas.slicesX = particleSystem3.textureAtlas.slicesY = 3;
		particleSystem3.gravityFactor = -0.2f;
	}
#endif

	{
		PROFILE_BLOCK("Light probe system");

		std::vector<vec4> lightProbePositions;
		for (float z = -20; z < 20; z += 10.f)
		{
			for (float y = 0; y < 30; y += 10.f)
			{
				for (float x = -70; x < 70; x += 10.f)
				{
					lightProbePositions.push_back(vec4(x, y, z, 1.f));
				}
			}
		}


		lightProbePositions.push_back(vec4(-1, -1, -1, 1) * 180);
		lightProbePositions.push_back(vec4( 1, -1, -1, 1) * 180);
		lightProbePositions.push_back(vec4(-1,  1, -1, 1) * 180);
		lightProbePositions.push_back(vec4( 1,  1, -1, 1) * 180);
		lightProbePositions.push_back(vec4(-1, -1,  1, 1) * 180);
		lightProbePositions.push_back(vec4( 1, -1,  1, 1) * 180);
		lightProbePositions.push_back(vec4(-1,  1,  1, 1) * 180);
		lightProbePositions.push_back(vec4( 1,  1,  1, 1) * 180);

		FILE* shFile = fopen("shs.txt", "r");
		if (shFile)
		{
			std::vector<spherical_harmonics> shs(lightProbePositions.size());

			for (spherical_harmonics& sh : shs)
			{
				for (uint32 i = 0; i < 9; ++i)
				{
					fscanf(shFile, "%f %f %f\n", &sh.coefficients[i].x, &sh.coefficients[i].y, &sh.coefficients[i].z);
					sh.coefficients[i].w = 1.f;
				}
				fscanf(shFile, "-----\n");
			}
			fclose(shFile);

			lightProbeSystem.initialize(device, commandList, lightingRT, lightProbePositions, shs);
		}
		else
		{
			lightProbeSystem.initialize(device, commandList, lightingRT, lightProbePositions);
		}
	}

	sun.worldSpaceDirection = comp_vec(-0.6f, -1.f, -0.3f, 0.f).normalize();
	sun.color = vec4(1.f, 0.93f, 0.76f, 0.f) * 50.f;

	sun.cascadeDistances.data[0] = 9.f;
	sun.cascadeDistances.data[1] = 39.f;
	sun.cascadeDistances.data[2] = 74.f;
	sun.cascadeDistances.data[3] = 10000.f;

	sun.bias = vec4(0.001f, 0.0015f, 0.0015f, 0.0035f);
	sun.blendArea = 0.07f;



	spotLight.worldSpacePosition = vec4(0.f, 5.f, 0.f, 1.f);
	spotLight.worldSpaceDirection = comp_vec(1.f, 0.f, 0.f, 0.f);
	spotLight.color = vec4(1.f, 1.f, 0.f, 0.f) * 50.f;

	spotLight.attenuation.linear = 0.f;
	spotLight.attenuation.quadratic = 0.015f;

	spotLight.outerAngle = DirectX::XMConvertToRadians(35.f);
	spotLight.innerAngle = DirectX::XMConvertToRadians(20.f);
	spotLight.bias = 0.001f;


	{
		PROFILE_BLOCK("Init gui");
		gui.initialize(device, commandList, screenRTFormats);
	}

	{
		PROFILE_BLOCK("Init debug display");
		debugDisplay.initialize(device, commandList, lightingRT);
	}

	{
		PROFILE_BLOCK("Load sponza model");

		cpu_triangle_mesh<vertex_3PUNTL> mesh;
		auto [submeshes, materials] = mesh.pushFromFile("res/sponza/sponza.obj");

		for (auto& vertex : mesh.vertices)
		{
			vec4 barycentric;
			vertex.lightProbeTetrahedronIndex = lightProbeSystem.getEnclosingTetrahedron(vertex.position * 0.03f, 0, barycentric);
		}

		indirectBuffer.push(mesh, submeshes, materials, createScaleMatrix(0.03f), commandList);
	}

	{
		PROFILE_BLOCK("Load sphere model");

		cpu_triangle_mesh<vertex_3PUNTL> mesh;
		submesh_info sphereSubmesh = mesh.pushSphere(21, 21, 1.f);

		vec4 colors[] = { vec4(1.f, 1.f, 1.f, 1.f), vec4(1.f, 1.f, 1.f, 1.f), vec4(1.f, 1.f, 1.f, 1.f), vec4(1.f, 1.f, 1.f, 1.f), vec4(1.f, 1.f, 1.f, 1.f) };
		float roughnesses[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };
		float metallics[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
		mat4 transforms[] = {
			createModelMatrix(vec3(-10.f + 0.f * 4.f, 3.f, 0.f), quat::identity),
			createModelMatrix(vec3(-10.f + 1.f * 4.f, 3.f, 0.f), quat::identity),
			createModelMatrix(vec3(-10.f + 2.f * 4.f, 3.f, 0.f), quat::identity),
			createModelMatrix(vec3(-10.f + 3.f * 4.f, 3.f, 0.f), quat::identity),
			createModelMatrix(vec3(-10.f + 4.f * 4.f, 3.f, 0.f), quat::identity),
		};

		// TODO: We cannot compute per vertex light probe indices for instanced objects.

		indirectBuffer.push(mesh, { sphereSubmesh }, colors, roughnesses, metallics, transforms, 5);
	}

	{
		PROFILE_BLOCK("Load flood light model");

		cpu_triangle_mesh<vertex_3PUNTL> mesh;
		auto [submeshes, materials] = mesh.pushFromFile("res/floodlight.fbx");

		mat4 model = createModelMatrix(vec3(spotLight.worldSpacePosition.x, 0.f, spotLight.worldSpacePosition.z - 1.f),
			createQuaternionFromAxisAngle(vec3(0.f, 1.f, 0.f), DirectX::XM_PIDIV2) *
			createQuaternionFromAxisAngle(vec3(1.f, 0.f, 0.f), -DirectX::XM_PIDIV2),
			0.03f);

		for (auto& vertex : mesh.vertices)
		{
			vec4 barycentric;
			vertex.lightProbeTetrahedronIndex = lightProbeSystem.getEnclosingTetrahedron(vertex.position * 0.03f, 0, barycentric);
		}

		indirectBuffer.push(mesh, submeshes, vec4(1.f, 1.f, 1.f, 1.f), 0.5f, 1.f, model);
	}

	{
		PROFILE_BLOCK("Big oak model");

		cpu_triangle_mesh<vertex_3PUNTL> mesh;
		auto [submeshes, materials] = mesh.pushFromFile("res/big_oak.obj");

		indirectBuffer.push(mesh, submeshes, materials, createScaleMatrix(1.f), commandList);
	}
	
	{
		PROFILE_BLOCK("Load environment");

		dx_texture equirectangular;
		{
			PROFILE_BLOCK("Load HDRI");
			commandList->loadTextureFromFile(equirectangular, L"res/hdris/sunset_in_the_chalk_quarry_4k_16bit.hdr", texture_type_noncolor);
			SET_NAME(equirectangular.resource, "Equirectangular map");
		}
		{
			PROFILE_BLOCK("Convert to cubemap");
			commandList->convertEquirectangularToCubemap(equirectangular, cubemap, 1024, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
			SET_NAME(cubemap.resource, "Skybox");
		}
		{
			PROFILE_BLOCK("Create irradiance map");
			commandList->createIrradianceMap(cubemap, irradiance);
			SET_NAME(irradiance.resource, "Global irradiance");
		}
		{
			PROFILE_BLOCK("Prefilter environment");
			commandList->prefilterEnvironmentMap(cubemap, prefilteredEnvironment, 256);
			SET_NAME(prefilteredEnvironment.resource, "Prefiltered global specular");
		}
	}

	indirectBuffer.finish(device, commandList, irradiance, prefilteredEnvironment, brdf, sunShadowMapTexture, sun.numShadowCascades,
		spotLightShadowMapTexture, lightProbeSystem);

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
		for (uint32 i = 0; i < indirectBuffer.indirectMaterials.size(); ++i)
		{
			commandList->transitionBarrier(indirectBuffer.indirectMaterials[i].albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->transitionBarrier(indirectBuffer.indirectMaterials[i].normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->transitionBarrier(indirectBuffer.indirectMaterials[i].roughness, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->transitionBarrier(indirectBuffer.indirectMaterials[i].metallic, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		commandList->transitionBarrier(densityMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		
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
	camera.farPlane = 1000.f;
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

		lightingRT.resize(width, height);
	}
}

void dx_game::update(float dt)
{
	camera.rotation = (createQuaternionFromAxisAngle(comp_vec(0.f, 1.f, 0.f), camera.yaw)
		* createQuaternionFromAxisAngle(comp_vec(1.f, 0.f, 0.f), camera.pitch)).normalize();

	camera.position = camera.position + camera.rotation * inputMovement * dt * CAMERA_MOVEMENT_SPEED * inputSpeedModifier;
	camera.updateMatrices(width, height);

#if ENABLE_PARTICLES
	particleSystemTime += dt;

	particleSystem1.spawnPosition.x = cos(particleSystemTime) * 20.f;
	particleSystem1.spawnPosition.y = sin(particleSystemTime) * 15.f + 20.f;
	particleSystem1.update(dt);

	particleSystem2.spawnPosition.x = cos(particleSystemTime + DirectX::XM_PI) * 20.f;
	particleSystem2.spawnPosition.y = sin(particleSystemTime + DirectX::XM_PI) * 15.f + 20.f;
	particleSystem2.update(dt);

	particleSystem3.update(dt);
#endif

	this->dt = dt;

	DEBUG_TAB(gui, "General")
	{
		gui.textF("Performance: %.2f fps (%.3f ms)", 1.f / dt, dt * 1000.f);
		DEBUG_GROUP(gui, "Camera")
		{
			gui.textF("Camera position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
			gui.textF("Input movement: %.2f, %.2f, %.2f", inputMovement.x, inputMovement.y, inputMovement.z);
			gui.slider("Near plane", camera.nearPlane, 0.1f, 10.f);
		}

		DEBUG_GROUP(gui, "Lighting")
		{
			DEBUG_GROUP(gui, "Sun")
			{
				gui.multislider("Cascade distances", sun.cascadeDistances.data, sun.numShadowCascades, 0.1f, 150.f, 0.1f);
				gui.slider("Cascade 0 bias", sun.bias.x, 0.f, 0.01f);
				gui.slider("Cascade 1 bias", sun.bias.y, 0.f, 0.01f);
				gui.slider("Cascade 2 bias", sun.bias.z, 0.f, 0.01f);
				gui.slider("Blend area", sun.blendArea, 0.f, 1.f);
			}

			DEBUG_GROUP(gui, "Spot light")
			{
				float angles[] = { DirectX::XMConvertToDegrees(spotLight.innerAngle), DirectX::XMConvertToDegrees(spotLight.outerAngle) };
				if (gui.multislider("Radii", angles, 2, 0.f, 90.f, 1.f))
				{
					spotLight.innerAngle = DirectX::XMConvertToRadians(angles[0]);
					spotLight.outerAngle = DirectX::XMConvertToRadians(angles[1]);
				}
				gui.slider("Linear attenuation", spotLight.attenuation.linear, 0.f, 3.f);
				gui.slider("Quadratic attenuation", spotLight.attenuation.quadratic, 0.f, 4.f);
				gui.slider("Bias", spotLight.bias, 0.f, 0.01f);
			}

			DEBUG_GROUP(gui, "Light probes")
			{
				gui.toggle("Show light probes", showLightProbes);
				gui.toggle("Show light probe connectivity", showLightProbeConnectivity);
				gui.toggle("Record light probes", lightProbeRecording);
			}
		}
		gui.textF("%u draw calls", indirectBuffer.numDrawCalls);
	}

	sun.updateMatrices(camera);
	spotLight.updateMatrices();
}

void dx_game::renderScene(dx_command_list* commandList, render_camera& camera)
{
	camera_cb cameraCB;
	camera.fillConstantBuffer(cameraCB);

	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress = commandList->uploadDynamicConstantBuffer(cameraCB);
	D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress = commandList->uploadDynamicConstantBuffer(sun);
	D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress = commandList->uploadDynamicConstantBuffer(spotLight);

#if DEPTH_PREPASS
	indirect.renderDepthOnly(commandList, camera, indirectBuffer);
#if ENABLE_PROCEDURAL
	indirect.renderDepthOnly(commandList, camera, indirectBuffer.indirectMesh, proceduralPlacement.depthOnlyCommandBuffer, 
		proceduralPlacement.maxNumDrawCalls, proceduralPlacement.numDrawCallsBuffer);
#endif
#endif

	commandList->transitionBarrier(lightProbeSystem.packedSphericalHarmonicsBuffer.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(lightProbeSystem.lightProbePositionBuffer.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(lightProbeSystem.lightProbeTetrahedraBuffer.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	indirect.render(commandList, indirectBuffer, cameraCBAddress, sunCBAddress, spotLightCBAddress);
#if ENABLE_PROCEDURAL
	indirect.render(commandList, indirectBuffer.indirectMesh, indirectBuffer.descriptors, proceduralPlacement.commandBuffer, 
		proceduralPlacement.maxNumDrawCalls, proceduralPlacement.numDrawCallsBuffer,
		cameraCBAddress, sunCBAddress, spotLightCBAddress);
#endif
	sky.render(commandList, cameraCBAddress, cubemap);
}

void dx_game::renderShadowmap(dx_command_list* commandList, dx_render_target& shadowMapRT, const mat4& vp)
{
	PROFILE_FUNCTION();

	commandList->setRenderTarget(shadowMapRT);
	commandList->setViewport(shadowMapRT.viewport);

	commandList->clearDepth(shadowMapRT.depthStencilAttachment->getDepthStencilView());

	// This only works, because the vertex shader expects the vp matrix as the first argument.
	commandList->setGraphics32BitConstants(INDIRECT_ROOTPARAM_CAMERA, vp);

	// Static scene.
	commandList->drawIndirect(
		indirect.depthOnlyCommandSignature,
		indirectBuffer.numDrawCalls,
		indirectBuffer.depthOnlyCommandBuffer);

#if ENABLE_PROCEDURAL && ENABLE_PROCEDURAL_SHADOWS
	// Procedurally generated scene.
	commandList->drawIndirect(
		indirect.depthOnlyCommandSignature,
		proceduralPlacement.maxNumDrawCalls,
		proceduralPlacement.numDrawCallsBuffer,
		proceduralPlacement.depthOnlyCommandBuffer);
#endif
}

uint64 dx_game::render(ComPtr<ID3D12Resource> backBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV)
{
#if ENABLE_PROCEDURAL
	placement_mesh placementMeshes[] =
	{
		{ 3984, 265919, 213153, 1572867 },
		{ 3648, 269903, 218635, 1638403 },
		{ 1920, 273551, 229579, 1703939 },
	};
	proceduralPlacement.generate(camera, densityMap, placementMeshes, arraysize(placementMeshes), dt);
#endif

	dx_command_list* commandList = dx_command_queue::renderCommandQueue.getAvailableCommandList();

	PIXSetMarker(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 0, 0), "Frame start.");

	commandList->setScissor(scissorRect);

	commandList->transitionBarrier(irradiance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(prefilteredEnvironment, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarrier(brdf, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Render to sun shadow map.
	{
		PROFILE_BLOCK("Record shadow map commands");

		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Sun shadow map.");

		// If more than the static scene is rendered here, this stuff must go in the loop.
		commandList->setPipelineState(indirect.depthOnlyPipelineState);
		commandList->setGraphicsRootSignature(indirect.depthOnlyRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->setVertexBuffer(0, indirectBuffer.indirectMesh.vertexBuffer);
		commandList->setIndexBuffer(indirectBuffer.indirectMesh.indexBuffer);

		for (uint32 i = 0; i < sun.numShadowCascades; ++i)
		{
			renderShadowmap(commandList, sunShadowMapRT[i], sun.vp[i]);
		}
		renderShadowmap(commandList, spotLightShadowMapRT, spotLight.vp);

		for (uint32 i = 0; i < sun.numShadowCascades; ++i)
		{
			commandList->transitionBarrier(sunShadowMapTexture[i],
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
		commandList->transitionBarrier(spotLightShadowMapTexture,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}


	if (lightProbeRecording)
	{
		if (lightProbeGlobalIndex < lightProbeSystem.lightProbePositions.size())
		{
			vec3 lightProbePosition = lightProbeSystem.lightProbePositions[lightProbeGlobalIndex].xyz;

			commandList->setRenderTarget(lightProbeSystem.lightProbeRT, 6 * lightProbeGlobalIndex + lightProbeFaceIndex);
			commandList->setViewport(lightProbeSystem.lightProbeRT.viewport);
			commandList->clearDepth(lightProbeSystem.lightProbeRT.depthStencilAttachment->getDepthStencilView());

			cubemap_camera lightProbeCamera;
			lightProbeCamera.initialize(lightProbePosition, lightProbeFaceIndex);
			renderScene(commandList, lightProbeCamera);
		}

		++lightProbeFaceIndex;
		if (lightProbeFaceIndex >= 6)
		{
			lightProbeFaceIndex = 0;
			++lightProbeGlobalIndex;
		}

		if (lightProbeGlobalIndex >= lightProbeSystem.lightProbePositions.size())
		{
			lightProbeRecording = false;
			lightProbeGlobalIndex = 0;
			lightProbeFaceIndex = 0;
		}
	}


	DEBUG_TAB(gui, "General")
	{
		gui.textF("%u/%u light probe faces recorded", 6 * lightProbeGlobalIndex + lightProbeFaceIndex, 6 * (uint32)lightProbeSystem.lightProbePositions.size());
		if (gui.button("Convert cubemaps to irradiance spherical harmonics"))
		{
			lightProbeSystem.tempSphericalHarmonicsBuffer.initialize<spherical_harmonics>(device, nullptr, (uint32)lightProbeSystem.lightProbePositions.size());

			uint32 index = 0;
			while (index < (uint32)lightProbeSystem.lightProbePositions.size())
			{
				dx_command_list* commandList = dx_command_queue::computeCommandQueue.getAvailableCommandList();

				//for (uint32 i = 0; i < 20 && index < (uint32)lightProbeSystem.lightProbePositions.size(); ++i, ++index)
				{
					dx_texture irradianceTemp;
					commandList->createIrradianceMap(lightProbeSystem.lightProbeHDRTexture, irradianceTemp, LIGHT_PROBE_RESOLUTION, index, -1.f);
					commandList->projectCubemapToSphericalHarmonics(irradianceTemp, lightProbeSystem.tempSphericalHarmonicsBuffer, 0, index);
				}

				++index;

				uint64 fenceValue = dx_command_queue::computeCommandQueue.executeCommandList(commandList);
				dx_command_queue::computeCommandQueue.waitForFenceValue(fenceValue);
			}
		}

		if (gui.button("Apply spherical harmonics"))
		{
			std::vector<spherical_harmonics> shs(lightProbeSystem.lightProbePositions.size());
			lightProbeSystem.tempSphericalHarmonicsBuffer.copyBackToCPU(shs.data(), (uint32)shs.size() * sizeof(spherical_harmonics));

			FILE* shFile = fopen("shs.txt", "w+");
			if (shFile)
			{
				for (const spherical_harmonics& sh : shs)
				{
					for (uint32 i = 0; i < 9; ++i)
					{
						fprintf(shFile, "%f %f %f\n", sh.coefficients[i].x, sh.coefficients[i].y, sh.coefficients[i].z);
					}
					fprintf(shFile, "-----\n");
				}
				fclose(shFile);
			}

			lightProbeSystem.setSphericalHarmonics(device, commandList, shs);
			commandList->transitionBarrier(lightProbeSystem.packedSphericalHarmonicsBuffer.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, true);
		}
	}

	commandList->setRenderTarget(lightingRT);
	commandList->setViewport(viewport);
	commandList->clearDepth(lightingRT.depthStencilAttachment->getDepthStencilView());

	renderScene(commandList, camera);

	if (isDebugCamera)
	{
		debugDisplay.renderFrustum(commandList, camera, mainCameraFrustum, vec4(1.f, 1.f, 1.f, 1.f));
	}

#if ENABLE_PARTICLES
	particles.renderParticleSystem(commandList, camera, particleSystem1);
	particles.renderParticleSystem(commandList, camera, particleSystem2);
	particles.renderParticleSystem(commandList, camera, particleSystem3);
#endif

	if (showLightProbes)
	{
		if (lightProbeSystem.tempSphericalHarmonicsBuffer.resource)
		{
			lightProbeSystem.visualizeLightProbes(commandList, camera, showLightProbes, showLightProbeConnectivity, debugDisplay);
		}
		else
		{
			lightProbeSystem.visualizeLightProbeCubemaps(commandList, camera, -1.f);
		}
	}

	// Transition back to common, so that copy and compute list can handle the resource (for readback and convolution).
	commandList->transitionBarrier(lightProbeSystem.packedSphericalHarmonicsBuffer.resource, D3D12_RESOURCE_STATE_COMMON);
	commandList->transitionBarrier(lightProbeSystem.lightProbeHDRTexture.resource, D3D12_RESOURCE_STATE_COMMON);
	commandList->transitionBarrier(lightProbeSystem.tempSphericalHarmonicsBuffer.resource, D3D12_RESOURCE_STATE_COMMON);






	// Transition backbuffer from "Present" to "Render Target", so we can render to it.
	commandList->transitionBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);


	commandList->setScreenRenderTarget(&screenRTV, 1, nullptr);

	present.render(commandList, hdrTexture);
	processAndDisplayProfileEvents(gui);
	gui.render(commandList, viewport); // Probably not completely correct here, since alpha blending assumes linear colors?

	// Transition back to "Present".
	commandList->transitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);

	return dx_command_queue::renderCommandQueue.executeCommandList(commandList);
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
	case key_tab:
	{
		isDebugCamera = !isDebugCamera;
		if (isDebugCamera)
		{
			mainCameraFrustum = camera.getWorldSpaceFrustum(20.f);
		}
	} break;
	}
	return true;
}

bool dx_game::mouseMoveCallback(mouse_move_event event)
{
	if (event.rightDown)
	{
		camera.pitch = camera.pitch - event.relDY * CAMERA_SENSITIVITY;
		camera.yaw = camera.yaw - event.relDX * CAMERA_SENSITIVITY;
	}
	return true;
}


