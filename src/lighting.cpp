#include "pch.h"
#include "lighting.h"
#include "error.h"
#include "graphics.h"
#include "profiling.h"

#include <tetgen/tetgen.h>

#include <pix3.h>

static uint32 packColorR11G11B10(vec4 c)
{
	const float range = 3.f;
	// For now. Let's see what the range will be.
	assert(c.x >= -range && c.x <= range);
	assert(c.y >= -range && c.y <= range);
	assert(c.z >= -range && c.z <= range);

	const uint32 rMax = (1 << 11) - 1;
	const uint32 gMax = (1 << 11) - 1;
	const uint32 bMax = (1 << 10) - 1;

	uint32 r = (uint32)remap(c.x, -range, range, 0.f, rMax);
	uint32 g = (uint32)remap(c.y, -range, range, 0.f, gMax);
	uint32 b = (uint32)remap(c.z, -range, range, 0.f, bMax);

	uint32 result = (r << 21) | (g << 10) | b;
	return result;
}

static vec4 unpackColorR11G11B10(uint32 c)
{
	const uint32 rMax = (1 << 11) - 1;
	const uint32 gMax = (1 << 11) - 1;
	const uint32 bMax = (1 << 10) - 1;

	float b = (float)(c & bMax);
	c >>= 10;
	float g = (float)(c & gMax);
	c >>= 11;
	float r = (float)c;

	return vec4(
		remap(r, 0.f, rMax, -1.f, 1.f),
		remap(g, 0.f, gMax, -1.f, 1.f),
		remap(b, 0.f, bMax, -1.f, 1.f),
		1.f);
}

static packed_spherical_harmonics packSphericalHarmonics(const spherical_harmonics& sh)
{
	packed_spherical_harmonics result;
	for (uint32 i = 0; i < 9; ++i)
	{
		result.coefficients[i] = packColorR11G11B10(sh.coefficients[i]);
	}
	return result;
}

void light_probe_system::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget,
	const std::vector<vec4>& lightProbePositions, const std::vector<spherical_harmonics>& sphericalHarmonics)
{
	DXGI_FORMAT hdrFormat = renderTarget.colorAttachments[0]->format;
	DXGI_FORMAT depthFormat = renderTarget.depthStencilAttachment->format;

	// Light probe cubemaps.
	CD3DX12_RESOURCE_DESC hdrTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrFormat, LIGHT_PROBE_RESOLUTION, LIGHT_PROBE_RESOLUTION);
	hdrTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	hdrTextureDesc.DepthOrArraySize = 6 * (uint32)lightProbePositions.size();
	hdrTextureDesc.MipLevels = 1;

	D3D12_CLEAR_VALUE hdrClearValue;
	hdrClearValue.Format = hdrTextureDesc.Format;
	hdrClearValue.Color[0] = 0.f;
	hdrClearValue.Color[1] = 0.f;
	hdrClearValue.Color[2] = 0.f;
	hdrClearValue.Color[3] = 0.f;

	lightProbeHDRTexture.initialize(device, hdrTextureDesc, &hdrClearValue);
	lightProbeRT.attachColorTexture(0, lightProbeHDRTexture);


	// Light probe depth.
	DXGI_FORMAT depthBufferFormat = depthFormat;
	CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat, LIGHT_PROBE_RESOLUTION, LIGHT_PROBE_RESOLUTION);
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	depthDesc.MipLevels = 1;

	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = { 1.f, 0 };

	lightProbeDepthTexture.initialize(device, depthDesc, &depthClearValue);
	lightProbeRT.attachDepthStencilTexture(lightProbeDepthTexture);




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

	setSphericalHarmonics(device, commandList, sphericalHarmonics);

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


		lightProbeTetrahedraBuffer.initialize(device, lightProbeTetrahedra.data(), (uint32)lightProbeTetrahedra.size(), commandList);
		lightProbePositionBuffer.initialize(device, lightProbePositions.data(), (uint32)lightProbePositions.size(), commandList);

		SET_NAME(lightProbeTetrahedraBuffer.resource, "Light probe tetrahedra");
		SET_NAME(lightProbePositionBuffer.resource, "Light probe positions");
	}

}

void light_probe_system::setSphericalHarmonics(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const std::vector<spherical_harmonics>& sphericalHarmonics)
{
	this->sphericalHarmonics = sphericalHarmonics;

	std::vector<packed_spherical_harmonics> packedSHs(sphericalHarmonics.size());
	for (uint32 i = 0; i < (uint32)sphericalHarmonics.size(); ++i)
	{
		packedSHs[i] = packSphericalHarmonics(sphericalHarmonics[i]);
	}

	if (!packedSphericalHarmonicsBuffer.resource)
	{
		packedSphericalHarmonicsBuffer.initialize(device, packedSHs.data(), (uint32)packedSHs.size(), commandList);
		SET_NAME(packedSphericalHarmonicsBuffer.resource, "Packed SHs");
	}
	else
	{
		commandList->uploadBufferData(packedSphericalHarmonicsBuffer.resource, packedSHs.data(), (uint32)packedSHs.size() * sizeof(packed_spherical_harmonics));
	}
}

void light_probe_system::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, const dx_render_target& renderTarget,
	const std::vector<vec4>& lightProbePositions)
{
	spherical_harmonics defaultSH =
	{
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
		vec4(0.f, 0.f, 0.f, 0.f),
	};

	std::vector<spherical_harmonics> sphericalHarmonics(lightProbePositions.size(), defaultSH);

#if 0
	for (uint32 i = 0; i < sphericalHarmonics.size(); ++i)
	{
		sphericalHarmonics[i].coefficients[0] = vec4(randomFloat(0.f, 1.5f), randomFloat(0.f, 1.5f), randomFloat(0.f, 1.5f), 0.f);
	}
#endif

	initialize(device, commandList, renderTarget, lightProbePositions, sphericalHarmonics);
}

vec4 light_probe_system::calculateBarycentricCoordinates(const light_probe_tetrahedron& tet, vec3 position)
{
	//PROFILE_FUNCTION();

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

spherical_harmonics light_probe_system::getInterpolatedSphericalHarmonics(uint32 tetrahedronIndex, vec4 barycentric)
{
	return getInterpolatedSphericalHarmonics(lightProbeTetrahedra[tetrahedronIndex], barycentric);
}

uint32 light_probe_system::getEnclosingTetrahedron(vec3 position, uint32 lastTetrahedron, vec4& barycentric)
{
	//PROFILE_FUNCTION();

	barycentric = calculateBarycentricCoordinates(lightProbeTetrahedra[lastTetrahedron], position);

	const uint32 maxNumIterations = 512;

	uint32 iterations = 0;

	while (!(barycentric.x >= 0.f && barycentric.y >= 0.f && barycentric.z >= 0.f && barycentric.w >= 0.f) && iterations < maxNumIterations)
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

		++iterations;
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
		camera.viewProjectionMatrix * createModelMatrix(position, quat::identity),
		uvzScale
	};
	commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
	commandList->setIndexBuffer(lightProbeMesh.indexBuffer);
	commandList->bindCubemap(VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE, 0, cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
}

void light_probe_system::visualizeLightProbeCubemaps(dx_command_list* commandList, const render_camera& camera, float uvzScale)
{
	PROFILE_FUNCTION();

	PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize light probe cubemaps.");

	commandList->setPipelineState(visualizeCubemapPipeline);
	commandList->setGraphicsRootSignature(visualizeCubemapRootSignature);

	commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
	commandList->setIndexBuffer(lightProbeMesh.indexBuffer);

	commandList->transitionBarrier(lightProbeHDRTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (uint32 i = 0; i < lightProbePositions.size(); ++i)
	{
		struct
		{
			mat4 mvp;
			float uvzScale;
		} cb = {
			camera.viewProjectionMatrix * createModelMatrix(lightProbePositions[i].xyz, quat::identity),
			uvzScale
		};
		commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = lightProbeHDRTexture.resource->GetDesc().Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		srvDesc.TextureCubeArray.MipLevels = 1;
		srvDesc.TextureCubeArray.NumCubes = 1;
		srvDesc.TextureCubeArray.First2DArrayFace = i * 6;

		commandList->setShaderResourceView(VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE, 0, lightProbeHDRTexture,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, &srvDesc);

		commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
	}
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
		camera.viewProjectionMatrix * createModelMatrix(position, quat::identity),
		uvzScale
	};
	commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

	commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
	commandList->setIndexBuffer(lightProbeMesh.indexBuffer);

	D3D12_GPU_VIRTUAL_ADDRESS shCBAddress = commandList->uploadDynamicConstantBuffer(sh);
	commandList->setGraphicsDynamicConstantBuffer(VISUALIZE_LIGHTPROBE_ROOTPARAM_TEXTURE, shCBAddress);

	commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
}

void light_probe_system::visualizeLightProbes(dx_command_list* commandList, const render_camera& camera, bool showProbes, bool showTetrahedralMesh,
	debug_display& debugDisplay)
{
	PROFILE_FUNCTION();

	if (showTetrahedralMesh)
	{
		debugDisplay.renderLineMesh(commandList, camera, tetrahedronMesh, mat4::identity);
	}

	if (showProbes)
	{
		PIXScopedEvent(commandList->getD3D12CommandList().Get(), PIX_COLOR(255, 255, 0), "Visualize SH light probe.");

		commandList->setPipelineState(visualizeSHBufferPipeline);
		commandList->setGraphicsRootSignature(visualizeSHBufferRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		commandList->setVertexBuffer(0, lightProbeMesh.vertexBuffer);
		commandList->setIndexBuffer(lightProbeMesh.indexBuffer);

		commandList->transitionBarrier(tempSphericalHarmonicsBuffer.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->stageDescriptors(VISUALIZE_LIGHTPROBE_ROOTPARAM_SH, 0, 1, tempSphericalHarmonicsBuffer.srv);

		// TODO: This could be rendered instanced.
		for (uint32 i = 0; i < lightProbePositions.size(); ++i)
		{
			struct
			{
				mat4 mvp;
				float uvzScale;
			} cb = {
				camera.viewProjectionMatrix * createModelMatrix(lightProbePositions[i], quat::identity),
				1.f
			};
			commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_CB, cb);

			commandList->setGraphics32BitConstants(VISUALIZE_LIGHTPROBE_ROOTPARAM_SH_INDEX, i);

			commandList->drawIndexed(lightProbeMesh.indexBuffer.numIndices, 1, 0, 0, 0);
		}
	}
}

void directional_light::updateMatrices(const render_camera& camera)
{
	comp_mat viewMatrix = createLookAt(vec3(0.f, 0.f, 0.f), worldSpaceDirection, vec3(0.f, 1.f, 0.f));

	vec3 worldForward = camera.rotation * vec3(0.f, 0.f, -1.f);
	camera_frustum worldFrustum = camera.getWorldSpaceFrustum();

	comp_vec worldBottomLeft = worldFrustum.farBottomLeft - worldFrustum.nearBottomLeft;
	comp_vec worldBottomRight = worldFrustum.farBottomRight - worldFrustum.nearBottomRight;
	comp_vec worldTopLeft = worldFrustum.farTopLeft - worldFrustum.nearTopLeft;
	comp_vec worldTopRight = worldFrustum.farTopRight - worldFrustum.nearTopRight;

	worldBottomLeft /= dot3(worldBottomLeft, worldForward);
	worldBottomRight /= dot3(worldBottomRight, worldForward);
	worldTopLeft /= dot3(worldTopLeft, worldForward);
	worldTopRight /= dot3(worldTopRight, worldForward);

	comp_vec worldEye = vec4(camera.position, 1.f);
	comp_vec sunEye = viewMatrix * worldEye;

	bounding_box initialBB = bounding_box::negativeInfinity();
	initialBB.grow(sunEye);

	for (uint32 i = 0; i < numShadowCascades; ++i)
	{
		float distance = cascadeDistances.data[i];

		comp_vec sunBottomLeft = viewMatrix * (worldEye + distance * worldBottomLeft);
		comp_vec sunBottomRight = viewMatrix * (worldEye + distance * worldBottomRight);
		comp_vec sunTopLeft = viewMatrix * (worldEye + distance * worldTopLeft);
		comp_vec sunTopRight = viewMatrix * (worldEye + distance * worldTopRight);

		bounding_box bb = initialBB;
		bb.grow(sunBottomLeft);
		bb.grow(sunBottomRight);
		bb.grow(sunTopLeft);
		bb.grow(sunTopRight);

		bb.expand(2.f);

		comp_mat projMatrix = createOrthographicMatrix(bb.min.x, bb.max.x, bb.max.y, bb.min.y, -bb.max.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -bb.min.z);

		vp[i] = projMatrix * viewMatrix;
	}

	texelSize = 1.f / (float)shadowMapDimensions;
}

void spot_light::updateMatrices()
{
	comp_mat viewMatrix = createLookAt(worldSpacePosition, worldSpacePosition + worldSpaceDirection, vec3(0.f, 1.f, 0.f));
	comp_mat projMatrix = createPerspectiveMatrix(outerAngle * 2.f, 1.f, 0.1f, 150.f);
	vp = projMatrix * viewMatrix;

	texelSize = 1.f / (float)shadowMapDimensions;
	outerCutoff = cos(outerAngle);
	innerCutoff = cos(innerAngle);
}
