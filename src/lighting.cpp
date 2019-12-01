#include "pch.h"
#include "lighting.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <tetgen/tetgen.h>

#include <pix3.h>


void light_probe_system::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget, const std::vector<vec3>& lightProbePositions)
{
	// Visualization pipelines.

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/visualize_light_probe_vs.cso", &vertexShaderBlob));

	{
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/visualize_light_probe_cubemap_ps.cso", &pixelShaderBlob));

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
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_CB].InitAsConstants(sizeof(mat4) / sizeof(float) + 1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // MVP matrix.
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.NumStaticSamplers = 1;
		visualizeCubemapRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = visualizeCubemapRootSignature.rootSignature.Get();
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
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&visualizeCubemapPipeline)));

		SET_NAME(visualizeCubemapRootSignature.rootSignature, "Visualize Cubemap Light Probe Root Signature");
		SET_NAME(visualizeCubemapPipeline, "Visualize Cubemap Light Probe Pipeline");
	}

	{
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/visualize_light_probe_sh_buffer_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 shs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_CB].InitAsConstants(sizeof(mat4) / sizeof(float) + 1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // MVP matrix.
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_SH].InitAsDescriptorTable(1, &shs, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_SH_INDEX].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL); // Light probe index.

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		visualizeSHBufferRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = visualizeSHBufferRootSignature.rootSignature.Get();
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
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&visualizeSHBufferPipeline)));

		SET_NAME(visualizeSHBufferRootSignature.rootSignature, "Visualize SH Light Probe Buffer Root Signature");
		SET_NAME(visualizeSHBufferPipeline, "Visualize SH Light Probe Buffer Pipeline");
	}

	{
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/visualize_light_probe_sh_direct_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_CB].InitAsConstants(sizeof(mat4) / sizeof(float) + 1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // MVP matrix.
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_SH].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		visualizeSHDirectRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = visualizeSHDirectRootSignature.rootSignature.Get();
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
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&visualizeSHDirectPipeline)));

		SET_NAME(visualizeSHDirectRootSignature.rootSignature, "Visualize SH Light Probe Direct Root Signature");
		SET_NAME(visualizeSHDirectPipeline, "Visualize SH Light Probe Direct Pipeline");
	}

	cpu_triangle_mesh<vertex_3P> sphere;
	sphere.pushSphere(51, 51, 1.f);
	lightProbeMesh.initialize(device, commandList, sphere);





	this->lightProbePositions = lightProbePositions;

#if 0
	spherical_harmonics pinkSH = 
	{
		vec4(1.f, 0.f, 1.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
	};

	sphericalHarmonics.resize(lightProbePositions.size(), pinkSH);
#else
	sphericalHarmonics.resize(lightProbePositions.size());
	memset(sphericalHarmonics.data(), 0, sphericalHarmonics.size() * sizeof(spherical_harmonics));
	for (uint32 i = 0; i < sphericalHarmonics.size(); ++i)
	{
		sphericalHarmonics[i].coefficients[0] = vec4(randomFloat(-0.5f, 0.5f), randomFloat(-0.5f, 0.5f), randomFloat(-0.5f, 0.5f), 0.f);
	}
#endif

	sphericalHarmonicsBuffer.initialize(device, sphericalHarmonics.data(), (uint32)sphericalHarmonics.size(), commandList);

	{
		tetgenio tetgenIn, tetgenOut;
		tetgenIn.numberofpoints = (int)lightProbePositions.size();
		double* points = new double[3 * lightProbePositions.size()];

		for (uint32 i = 0; i < lightProbePositions.size(); ++i)
		{
			points[i * 3 + 0] = (double)lightProbePositions[i].x;
			points[i * 3 + 1] = (double)lightProbePositions[i].y;
			points[i * 3 + 2] = (double)lightProbePositions[i].z;
		}

		tetgenIn.pointlist = points; // Takes ownership.

		 // z = All indices start at zero. n = Generate neighbors. Q = Quiet. f = Generate faces. e = Generate edges.
		char* switches = (char*)"znQfe";

		tetrahedralize(switches, &tetgenIn, &tetgenOut);


#if 0
		FILE* file = fopen("res/tetrahedron.off", "w+");
		if (file)
		{
			fprintf(file,
				"OFF\n"
				"%d 0 %d\n",
				(int32)lightProbePositions.size(),
				(int32)tetgenOut.numberofedges
			);

			for (int i = 0; i < lightProbePositions.size(); ++i)
			{
				fprintf(file, "%f %f %f\n", lightProbePositions[i].x, lightProbePositions[i].y, lightProbePositions[i].z);
			}

			for (int i = 0; i < tetgenOut.numberofedges; ++i)
			{
				int a = tetgenOut.edgelist[i * 2 + 0];
				int b = tetgenOut.edgelist[i * 2 + 1];

				fprintf(file, "2 %d %d\n", a, b);
			}

			fclose(file);
		}
#endif

		lightProbeTetrahedra.resize(tetgenOut.numberoftetrahedra);
		for (uint32 i = 0; i < (uint32)tetgenOut.numberoftetrahedra; ++i)
		{
			light_probe_tetrahedron& tet = lightProbeTetrahedra[i];

			tet.a = tetgenOut.tetrahedronlist[i * 4 + 0];
			tet.b = tetgenOut.tetrahedronlist[i * 4 + 1];
			tet.c = tetgenOut.tetrahedronlist[i * 4 + 2];
			tet.d = tetgenOut.tetrahedronlist[i * 4 + 3];

			tet.na = tetgenOut.neighborlist[i * 4 + 0];
			tet.nb = tetgenOut.neighborlist[i * 4 + 1];
			tet.nc = tetgenOut.neighborlist[i * 4 + 2];
			tet.nd = tetgenOut.neighborlist[i * 4 + 3];

			vec3 c0 = lightProbePositions[tet.a] - lightProbePositions[tet.d];
			vec3 c1 = lightProbePositions[tet.b] - lightProbePositions[tet.d];
			vec3 c2 = lightProbePositions[tet.c] - lightProbePositions[tet.d];

			mat4 mat(c0.x, c1.x, c2.x, 0.f,
				c0.y, c1.y, c2.y, 0.f,
				c0.z, c1.z, c2.z, 0.f,
				0.f, 0.f, 0.f, 1.f);

			tet.matrix = mat.invert();
		}

		std::vector<indexed_line16> edgeList(tetgenOut.numberofedges);
		for (uint32 i = 0; i < (uint32)tetgenOut.numberofedges; ++i)
		{
			edgeList[i].a = tetgenOut.edgelist[i * 2 + 0];
			edgeList[i].b = tetgenOut.edgelist[i * 2 + 1];
		}

		tetrahedronMesh.vertexBuffer.initialize(device, lightProbePositions.data(), (uint32)lightProbePositions.size(), commandList);
		tetrahedronMesh.indexBuffer.initialize(device, (uint16*)edgeList.data(), (uint32)edgeList.size() * 2, commandList);
	}


	// TODO: Factor this out into debug display system.
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_flat_vs.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/unlit_flat_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Root signature.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;


		CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[VISUALIZE_LIGHTPROBE_ROOTPARAM_CB].InitAsConstants(sizeof(mat4) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // MVP matrix.

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		unlitRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = unlitRootSignature.rootSignature.Get();
		pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
		pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.dsvFormat = renderTarget.depthStencilFormat;
		pipelineStateStream.rtvFormats = renderTarget.renderTargetFormat;
		pipelineStateStream.rasterizer = defaultRasterizerDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(pipeline_state_stream), &pipelineStateStream
		};
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&unlitPipeline)));

		SET_NAME(unlitRootSignature.rootSignature, "Unlit Line Root Signature");
		SET_NAME(unlitPipeline, "Unlit Line Pipeline");
	}
}

vec4 light_probe_system::calculateBarycentricCoordinates(const light_probe_tetrahedron& tet, vec3 position)
{
	PROFILE_FUNCTION();

	vec4 barycentric = tet.matrix * (position - lightProbePositions[tet.d]);
	barycentric.w = 1.f - barycentric.x - barycentric.y - barycentric.z;
	return barycentric;
}

spherical_harmonics light_probe_system::getInterpolatedSphericalHarmonics(const light_probe_tetrahedron& tet, vec4 barycentric)
{
	PROFILE_FUNCTION();

	const spherical_harmonics& a = sphericalHarmonics[tet.a];
	const spherical_harmonics& b = sphericalHarmonics[tet.b];
	const spherical_harmonics& c = sphericalHarmonics[tet.c];
	const spherical_harmonics& d = sphericalHarmonics[tet.d];

	spherical_harmonics result;
	for (uint32 i = 0; i < 9; ++i)
	{
		result.coefficients[i] = barycentric.x * a.coefficients[i]
			+ barycentric.y * b.coefficients[i]
			+ barycentric.z * c.coefficients[i]
			+ barycentric.w * d.coefficients[i];
	}
	return result;
}

uint32 light_probe_system::getEnclosingTetrahedron(vec3 position, uint32 lastTetrahedron, vec4& barycentric)
{
	PROFILE_FUNCTION();

	barycentric = calculateBarycentricCoordinates(lightProbeTetrahedra[lastTetrahedron], position);

	while (!(barycentric.x >= 0.f && barycentric.y >= 0.f && barycentric.z >= 0.f && barycentric.w >= 0.f))
	{
		uint32 smallestIndex = 0;
		float smallest = barycentric.x;
		for (uint32 i = 1; i < 4; ++i)
		{
			if (barycentric.data[i] < smallest)
			{
				smallest = barycentric.data[i];
				smallestIndex = i;
			}
		}
		assert(smallest < 0.f);

		int neighbor = lightProbeTetrahedra[lastTetrahedron].neighbors[smallestIndex];

		if (neighbor != -1)
		{
			lastTetrahedron = (uint32)neighbor;
			barycentric = calculateBarycentricCoordinates(lightProbeTetrahedra[lastTetrahedron], position);
		}
		else
		{
			break;
		}
	}

	return lastTetrahedron;
}

void light_probe_system::visualizeCubemap(dx_command_list* commandList, const render_camera& camera, vec3 position, dx_texture& cubemap,
	float uvzScale)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize cubemap light probe.");

	commandList->setPipelineState(visualizeCubemapPipeline);
	commandList->setGraphicsRootSignature(visualizeCubemapRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	struct
	{
		mat4 mvp;
		float uvzScale;
	} cb = {
		camera.projectionMatrix * camera.viewMatrix * createModelMatrix(position, quat::identity),
		uvzScale
	};
	commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
	commandList->setIndexBuffer(lightProbeMesh.indexBuffer);
	commandList->bindCubemap(VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE, 0, cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
}

void light_probe_system::visualizeSH(dx_command_list* commandList, const render_camera& camera, vec3 position, const spherical_harmonics& sh, float uvzScale)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize SH light probe.");

	commandList->setPipelineState(visualizeSHDirectPipeline);
	commandList->setGraphicsRootSignature(visualizeSHDirectRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	struct
	{
		mat4 mvp;
		float uvzScale;
	} cb = {
		camera.projectionMatrix * camera.viewMatrix * createModelMatrix(position, quat::identity),
		uvzScale
	};
	commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
	commandList->setIndexBuffer(lightProbeMesh.indexBuffer);

	D3D12_GPU_VIRTUAL_ADDRESS shCBAddress = commandList->uploadDynamicConstantBuffer(sh);
	commandList->setGraphicsDynamicConstantBuffer(VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE, shCBAddress);

	commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
}

void light_probe_system::visualizeLightProbes(dx_command_list* commandList, const render_camera& camera, bool showProbes, bool showTetrahedralMesh)
{
	PROFILE_FUNCTION();

	if (showTetrahedralMesh)
	{
		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize light probe tetrahedra.");

		commandList->setPipelineState(unlitPipeline);
		commandList->setGraphicsRootSignature(unlitRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		mat4 vp = camera.projectionMatrix * camera.viewMatrix;
		commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, vp);

		commandList->setVertexBuffer(0, tetrahedronMesh.vertexBuffer);
		commandList->setIndexBuffer(tetrahedronMesh.indexBuffer);

		commandList->drawIndexed(tetrahedronMesh.indexBuffer.numIndices, 1, 0, 0, 0);
	}

	if (showProbes)
	{
		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize SH light probe.");

		commandList->setPipelineState(visualizeSHBufferPipeline);
		commandList->setGraphicsRootSignature(visualizeSHBufferRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
		commandList->setIndexBuffer(lightProbeMesh.indexBuffer);

		commandList->transitionBarrier(sphericalHarmonicsBuffer.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->stageDescriptors(VISUALIZE_LIGHTPROBE_ROOTPARAM_SH, 0, 1, sphericalHarmonicsBuffer.srv);

		// TODO: This could be rendered instanced.
		for (uint32 i = 0; i < lightProbePositions.size(); ++i)
		{
			struct
			{
				mat4 mvp;
				float uvzScale;
			} cb = {
				camera.projectionMatrix * camera.viewMatrix * createModelMatrix(lightProbePositions[i], quat::identity),
				1.f
			};
			commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

			commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_SH_INDEX, i);

			commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
		}
	}
}
