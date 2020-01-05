#pragma once

#include "command_list.h"
#include "material.h"
#include "lighting.h"
#include "camera.h"
#include "descriptor_heap.h"

#define INDIRECT_ROOTPARAM_CAMERA			0
#define INDIRECT_ROOTPARAM_MATERIAL			1
#define INDIRECT_ROOTPARAM_BRDF_TEXTURES	2
#define INDIRECT_ROOTPARAM_ALBEDOS			3
#define INDIRECT_ROOTPARAM_NORMALS			4
#define INDIRECT_ROOTPARAM_ROUGHNESSES		5
#define INDIRECT_ROOTPARAM_METALLICS		6
#define INDIRECT_ROOTPARAM_DIRECTIONAL		7
#define INDIRECT_ROOTPARAM_SPOT				8
#define INDIRECT_ROOTPARAM_SHADOWMAPS		9
#define INDIRECT_ROOTPARAM_LIGHTPROBES		10


#define DEPTH_PREPASS 1


#pragma pack(push, 1)
struct indirect_command
{
	material_cb material;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct indirect_depth_only_command
{
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	uint32 padding[3];
};
#pragma pack(pop)

struct indirect_descriptor_heap
{
	dx_descriptor_heap descriptorHeap;
	CD3DX12_GPU_DESCRIPTOR_HANDLE albedosOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE normalsOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE roughnessesOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE metallicsOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE brdfOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE shadowMapsOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE lightProbeOffset;
};

struct indirect_draw_buffer
{
	void push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes, std::vector<submesh_material_info>& materialInfos, 
		const mat4& transform, dx_command_list* commandList);
	void push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes, vec4 color, float roughness, float metallic,
		const mat4& transform);
	void push(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info> submeshes, 
		vec4* colors, float* roughnesses, float* metallics, const mat4* transforms, uint32 instanceCount);


	void finish(ComPtr<ID3D12Device2> device, dx_command_list* commandList,
		dx_texture& irradiance, dx_texture& prefilteredEnvironment, dx_texture& brdf,
		dx_texture* sunShadowMapCascades, uint32 numSunShadowMapCascades,
		dx_texture& spotLightShadowMap,
		light_probe_system& lightProbeSystem);

	cpu_triangle_mesh<vertex_3PUNTL> cpuMesh;

	dx_mesh indirectMesh;
	std::vector<dx_material> indirectMaterials;

	std::vector<indirect_command> commands;
	std::vector<indirect_depth_only_command> depthOnlyCommands;

	dx_buffer commandBuffer;
	dx_buffer depthOnlyCommandBuffer;
	uint32 numDrawCalls = 0;

	indirect_descriptor_heap descriptors;

private:
	void pushInternal(cpu_triangle_mesh<vertex_3PUNTL>& mesh, std::vector<submesh_info>& submeshes);
};

struct indirect_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget, DXGI_FORMAT shadowMapFormat);

	void render(dx_command_list* commandList, indirect_draw_buffer& indirectBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress); 

	void render(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
		dx_buffer& commandBuffer, uint32 maxNumDrawCalls, dx_buffer& numDrawCallsBuffer, dx_vertex_buffer& instanceBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, 
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress, 
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress);

	void render(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
		dx_buffer& commandBuffer, uint32 numDrawCalls,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress);


	void renderDepthOnly(dx_command_list* commandList, const render_camera& camera, indirect_draw_buffer& indirectBuffer);
	void renderDepthOnly(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh,
		dx_buffer& depthOnlyCommandBuffer, uint32 numDrawCalls); 
	void renderDepthOnly(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh,
		dx_buffer& depthOnlyCommandBuffer, uint32 maxNumDrawCalls, dx_buffer& numDrawCallsBuffer, dx_vertex_buffer& instanceBuffer);


	ComPtr<ID3D12PipelineState> geometryPipelineState;
	dx_root_signature geometryRootSignature;
	ComPtr<ID3D12CommandSignature> geometryCommandSignature;

	ComPtr<ID3D12PipelineState> depthOnlyPipelineState;
	dx_root_signature depthOnlyRootSignature;
	ComPtr<ID3D12CommandSignature> depthOnlyCommandSignature;

private:
	void setupPipeline(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
		dx_buffer& commandBuffer, 
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress);

	void setupDepthOnlyPipeline(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh, dx_buffer& commandBuffer);
};
