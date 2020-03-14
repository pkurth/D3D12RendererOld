#include "pch.h"
#include "tree.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <pix3.h>


#define TREE_ROOTPARAM_CAMERA			0
#define TREE_ROOTPARAM_SKIN				1
#define TREE_ROOTPARAM_MATERIAL			2
#define TREE_ROOTPARAM_BRDF_TEXTURES	3
#define TREE_ROOTPARAM_ALBEDOS			4
#define TREE_ROOTPARAM_NORMALS			5
#define TREE_ROOTPARAM_ROUGHNESSES		6
#define TREE_ROOTPARAM_METALLICS		7
#define TREE_ROOTPARAM_DIRECTIONAL		8
#define TREE_ROOTPARAM_SPOT				9
#define TREE_ROOTPARAM_SHADOWMAPS		10
#define TREE_ROOTPARAM_LIGHTPROBES		11


void tree_pipeline::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget, DXGI_FORMAT shadowMapFormat)
{
	PROFILE_FUNCTION();

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/tree_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/standard_ps.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "LIGHTPROBE_TETRAHEDRON", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SKINNING_INDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SKINNING_WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 pbrTextures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1);

	CD3DX12_DESCRIPTOR_RANGE1 albedos(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 2);
	CD3DX12_DESCRIPTOR_RANGE1 normals(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 3);
	CD3DX12_DESCRIPTOR_RANGE1 roughnesses(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 4);
	CD3DX12_DESCRIPTOR_RANGE1 metallics(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UNBOUNDED_DESCRIPTOR_RANGE, 0, 5);

	CD3DX12_DESCRIPTOR_RANGE1 shadowMaps(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_NUM_SUN_SHADOW_CASCADES + 1, 0, 6); // Sun cascades + spot light.

	CD3DX12_DESCRIPTOR_RANGE1 pointLights(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MAX_NUM_SUN_SHADOW_CASCADES, 6);

	CD3DX12_DESCRIPTOR_RANGE1 lightProbes[] =
	{
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 7), // Positions.
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 7), // Spherical harmonics.
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 7), // Tetrahedra.
	};

	CD3DX12_ROOT_PARAMETER1 rootParameters[12];
	rootParameters[TREE_ROOTPARAM_CAMERA].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL); // Camera.
	rootParameters[TREE_ROOTPARAM_SKIN].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // Skin.

	rootParameters[TREE_ROOTPARAM_MATERIAL].InitAsConstants(sizeof(material_cb) / sizeof(float), 2, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Material.

	// PBR.
	rootParameters[TREE_ROOTPARAM_BRDF_TEXTURES].InitAsDescriptorTable(1, &pbrTextures, D3D12_SHADER_VISIBILITY_PIXEL);

	// Materials.
	rootParameters[TREE_ROOTPARAM_ALBEDOS].InitAsDescriptorTable(1, &albedos, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[TREE_ROOTPARAM_NORMALS].InitAsDescriptorTable(1, &normals, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[TREE_ROOTPARAM_ROUGHNESSES].InitAsDescriptorTable(1, &roughnesses, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[TREE_ROOTPARAM_METALLICS].InitAsDescriptorTable(1, &metallics, D3D12_SHADER_VISIBILITY_PIXEL);

	// Sun.
	rootParameters[TREE_ROOTPARAM_DIRECTIONAL].InitAsConstantBufferView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	// Spot light.
	rootParameters[TREE_ROOTPARAM_SPOT].InitAsConstantBufferView(4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	// Shadow maps.
	rootParameters[TREE_ROOTPARAM_SHADOWMAPS].InitAsDescriptorTable(1, &shadowMaps, D3D12_SHADER_VISIBILITY_PIXEL);

	// Light probes.
	rootParameters[TREE_ROOTPARAM_LIGHTPROBES].InitAsDescriptorTable(arraysize(lightProbes), lightProbes, D3D12_SHADER_VISIBILITY_PIXEL);



	CD3DX12_STATIC_SAMPLER_DESC samplers[] =
	{
		staticLinearWrapSampler(0),
		staticLinearClampSampler(1),
		staticShadowMapSampler(2),
	};

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = arraysize(samplers);
	lightingRootSignature.initialize(device, rootSignatureDesc, false);

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

		pipelineStateStream.rootSignature = lightingRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
		pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
		pipelineStateStream.rasterizer = noBackfaceCullRasterizerDesc;
#if DEPTH_PREPASS
		pipelineStateStream.depthStencil = equalDepthDesc;
#endif

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&lightingPipelineState)));
	}

	// Depth only pass (for depth pre pass and shadow maps).
	rootParameters[TREE_ROOTPARAM_CAMERA].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // VP matrix.
	rootSignatureDesc.NumParameters = 2; // Camera and skin.
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
		pipelineStateStream.rasterizer = noBackfaceCullRasterizerDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&depthOnlyPipelineState)));
	}


	SET_NAME(lightingRootSignature.rootSignature, "Tree Root Signature");
	SET_NAME(lightingPipelineState, "Tree Pipeline");
	SET_NAME(depthOnlyRootSignature.rootSignature, "Tree Depth Only Root Signature");
	SET_NAME(depthOnlyPipelineState, "Tree Depth Only Pipeline");


	cpu_triangle_mesh<vertex_3PUNTLW> treeMesh;
	submeshes = treeMesh.pushFromFile("res/trees/tree.fbx", &skeleton);
	mesh.initialize(device, commandList, treeMesh);
}

static float treeTime = 0.f;

struct tree_skin_cb
{
	mat4 foot;
	mat4 trunk;
};

static tree_skin_cb getSkin(const animation_skeleton& skeleton)
{
	quat r1 = comp_quat(vec3(0.f, 0.f, 1.f), sin(treeTime)  * 0.5f);
	quat r2 = comp_quat(vec3(0.f, 0.f, 1.f), -sin(treeTime) * 0.5f);

	trs localTransforms[] =
	{
		trs(vec3(0.f, 0.f, 0.f), r1, 1.f),
		trs(r1 * vec3(0.f, -skeleton.skeletonJoints[1].invBindMatrix.m13, 0.f), r2, 1.f),
	};

	tree_skin_cb result;

	trs globalTransforms[2];
	skeleton.getGlobalTransforms(localTransforms, globalTransforms, trs::identity);
	skeleton.getSkinningMatrices(globalTransforms, (mat4*)&result);

	return result;
}

void tree_pipeline::render(dx_command_list* commandList, 
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
	D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress,
	const indirect_descriptor_heap& descriptors)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Setup tree pipeline.");

	tree_skin_cb treeSkinCB = getSkin(skeleton);
	treeTime += 0.01f;

	D3D12_GPU_VIRTUAL_ADDRESS skinCBAddress = commandList->uploadDynamicConstantBuffer(treeSkinCB);

	commandList->setPipelineState(lightingPipelineState);
	commandList->setGraphicsRootSignature(lightingRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setGraphicsDynamicConstantBuffer(TREE_ROOTPARAM_CAMERA, cameraCBAddress);
	commandList->setGraphicsDynamicConstantBuffer(TREE_ROOTPARAM_SKIN, skinCBAddress);

	commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptors.descriptorHeap.descriptorHeap);

	// PBR.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_BRDF_TEXTURES, descriptors.brdfOffset);

	// Materials.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_ALBEDOS, descriptors.albedosOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_NORMALS, descriptors.normalsOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_ROUGHNESSES, descriptors.roughnessesOffset);
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_METALLICS, descriptors.metallicsOffset);

	// Sun.
	commandList->setGraphicsDynamicConstantBuffer(TREE_ROOTPARAM_DIRECTIONAL, sunCBAddress);
	commandList->setGraphicsDynamicConstantBuffer(TREE_ROOTPARAM_SPOT, spotLightCBAddress);

	// Shadow maps.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_SHADOWMAPS, descriptors.shadowMapsOffset);

	// Light probes.
	commandList->getD3D12CommandList()->SetGraphicsRootDescriptorTable(TREE_ROOTPARAM_LIGHTPROBES, descriptors.lightProbeOffset);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);

	for (uint32 i = 0; i < (uint32)submeshes.size(); ++i)
	{
		material_cb material;
		material.textureID_usageFlags = 0;
		if (i == 1)
		{
			material.albedoTint = vec4(0.065449f, 0.8f, 0.015439f, 1.f);
		}
		else
		{
			material.albedoTint = vec4(0.12371f, 0.032713f, 0.0151f, 1.f);
		}
		material.metallicOverride = 0.f;
		material.roughnessOverride = 1.f;
		commandList->setGraphics32BitConstants(TREE_ROOTPARAM_MATERIAL, material);

		submesh_info& sm = submeshes[i];
		commandList->drawIndexed(sm.numTriangles * 3, 1, sm.firstTriangle * 3, sm.baseVertex, 0);
	}

	commandList->resetToDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void tree_pipeline::renderDepthOnly(dx_command_list* commandList, const render_camera& camera)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Setup depth only tree pipeline.");

	commandList->setPipelineState(depthOnlyPipelineState);
	commandList->setGraphicsRootSignature(depthOnlyRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setVertexBuffer(0, mesh.vertexBuffer);
	commandList->setIndexBuffer(mesh.indexBuffer);

	tree_skin_cb treeSkinCB = getSkin(skeleton);

	D3D12_GPU_VIRTUAL_ADDRESS skinCBAddress = commandList->uploadDynamicConstantBuffer(treeSkinCB);

	// This only works, because the vertex shader expects the vp matrix as the first argument.
	commandList->setGraphics32BitConstants(INDIRECT_ROOTPARAM_CAMERA, camera.viewProjectionMatrix);
	commandList->setGraphicsDynamicConstantBuffer(TREE_ROOTPARAM_SKIN, skinCBAddress);

	for (uint32 i = 0; i < (uint32)submeshes.size(); ++i)
	{
		submesh_info& sm = submeshes[i];
		commandList->drawIndexed(sm.numTriangles * 3, 1, sm.firstTriangle * 3, sm.baseVertex, 0);
	}
}
