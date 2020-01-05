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

struct submesh_identifier
{
	uint32 firstTriangle;
	uint32 numTriangles;
	uint32 baseVertex;
	uint32 textureID_usageFlags;
	float roughnessOverride;
	float metallicOverride;
	vec4 albedoTint;

	submesh_identifier(submesh_info info)
		: submesh_identifier(info, vec4(1.f, 1.f, 1.f, 1.f), 1.f, 0.f)
	{}

	submesh_identifier(submesh_info info, vec4 albedoTint, float roughness, float metallic)
		: firstTriangle(info.firstTriangle)
		, numTriangles(info.numTriangles)
		, baseVertex(info.baseVertex)
		, textureID_usageFlags(info.textureID_usageFlags)
		, roughnessOverride(roughness)
		, metallicOverride(metallic)
		, albedoTint(albedoTint)
	{}

	bool operator==(const submesh_identifier& other) const
	{
		return firstTriangle == other.firstTriangle
			&& numTriangles == other.numTriangles
			&& baseVertex == other.baseVertex
			&& textureID_usageFlags == other.textureID_usageFlags
			&& roughnessOverride == other.roughnessOverride
			&& metallicOverride == other.metallicOverride
			&& albedoTint.x == other.albedoTint.x
			&& albedoTint.y == other.albedoTint.y
			&& albedoTint.z == other.albedoTint.z
			&& albedoTint.w == other.albedoTint.w;
	}
};

namespace std
{
	template<>
	struct hash<submesh_identifier>
	{
		std::size_t operator()(const submesh_identifier& id) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, id.firstTriangle);
			hash_combine(seed, id.numTriangles);
			hash_combine(seed, id.baseVertex);
			hash_combine(seed, id.textureID_usageFlags);
			hash_combine(seed, id.roughnessOverride);
			hash_combine(seed, id.metallicOverride);
			hash_combine(seed, id.albedoTint.x);
			hash_combine(seed, id.albedoTint.y);
			hash_combine(seed, id.albedoTint.z);
			hash_combine(seed, id.albedoTint.w);

			return seed;
		}
	};
}

struct indirect_draw_buffer
{
	void initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList,
		dx_texture& irradiance, dx_texture& prefilteredEnvironment, dx_texture& brdf,
		dx_texture* sunShadowMapCascades, uint32 numSunShadowMapCascades,
		dx_texture& spotLightShadowMap,
		light_probe_system& lightProbeSystem,
		cpu_triangle_mesh<vertex_3PUNTL>& mesh);

	void pushInstance(submesh_info submesh, mat4 transform);
	void pushInstance(std::vector<submesh_info>& submeshes, mat4 transform);

	void pushInstance(submesh_info submesh, mat4 transform, vec4 albedoTint, float roughnessOverride, float metallicOverride);

	void finish(dx_command_list* commandList);

	dx_mesh indirectMesh;
	std::vector<dx_material> indirectMaterials;

	dx_buffer commandBuffer;
	dx_buffer depthOnlyCommandBuffer;
	dx_vertex_buffer instanceBuffer;
	uint32 numDrawCalls = 0;

	indirect_descriptor_heap descriptors;

	ComPtr<ID3D12Device2> device;

private:

	std::unordered_map<submesh_identifier, std::vector<mat4>> instances;
};

struct indirect_pipeline
{
	void initialize(ComPtr<ID3D12Device2> device, const dx_render_target& renderTarget, DXGI_FORMAT shadowMapFormat);

	void render(dx_command_list* commandList, indirect_draw_buffer& indirectBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress); 

	void render(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
		dx_buffer& commandBuffer, uint32 numDrawCalls, dx_vertex_buffer& instanceBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress, 
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress, 
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress);


	void renderDepthOnly(dx_command_list* commandList, const render_camera& camera, indirect_draw_buffer& indirectBuffer);
	void renderDepthOnly(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh,
		dx_buffer& depthOnlyCommandBuffer, uint32 numDrawCalls, dx_vertex_buffer& instanceBuffer);


	ComPtr<ID3D12PipelineState> geometryPipelineState;
	dx_root_signature geometryRootSignature;
	ComPtr<ID3D12CommandSignature> geometryCommandSignature;

	ComPtr<ID3D12PipelineState> depthOnlyPipelineState;
	dx_root_signature depthOnlyRootSignature;
	ComPtr<ID3D12CommandSignature> depthOnlyCommandSignature;

private:
	void setupPipeline(dx_command_list* commandList, dx_mesh& mesh, indirect_descriptor_heap& descriptors,
		dx_buffer& commandBuffer, 
		dx_vertex_buffer& instanceBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cameraCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS sunCBAddress,
		D3D12_GPU_VIRTUAL_ADDRESS spotLightCBAddress);

	void setupDepthOnlyPipeline(dx_command_list* commandList, const render_camera& camera, dx_mesh& mesh, dx_buffer& commandBuffer,
		dx_vertex_buffer& instanceBuffer);
};
