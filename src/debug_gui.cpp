#include "pch.h"
#include "debug_gui.h"
#include "error.h"
#include "graphics.h"

void debug_gui::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, D3D12_RT_FORMAT_ARRAY rtvFormats)
{
	resizeIndexBuffer(commandList, 2048);
	yOffset = 1;
	level = 0;
	font.initialize(commandList, "arial", 25, true);
	textHeight = font.height * 0.75f;

	ComPtr<ID3DBlob> vertexShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/font_vs.cso", &vertexShaderBlob));
	ComPtr<ID3DBlob> pixelShaderBlob;
	checkResult(D3DReadFileToBlob(L"shaders/bin/font_ps.cso", &pixelShaderBlob));

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


	// Root signature.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_DESCRIPTOR_RANGE1 textures(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Inv screen dimensions.
	rootParameters[1].InitAsDescriptorTable(1, &textures, D3D12_SHADER_VISIBILITY_PIXEL); // Texture.

	CD3DX12_STATIC_SAMPLER_DESC sampler = staticLinearClampSampler(0);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = rootSignatureFlags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = arraysize(rootParameters);
	rootSignatureDesc.pStaticSamplers = &sampler;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignature.initialize(device, rootSignatureDesc);


	struct pipeline_state_stream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 depthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
	} pipelineStateStream;

	pipelineStateStream.rootSignature = rootSignature.rootSignature.Get();
	pipelineStateStream.inputLayout = { inputLayout, arraysize(inputLayout) };
	pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.rtvFormats = rtvFormats;

	CD3DX12_DEPTH_STENCIL_DESC1 depthDesc(D3D12_DEFAULT);
	depthDesc.DepthEnable = false; // Don't do depth-check.
	depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth (or stencil) buffer.
	pipelineStateStream.depthStencilDesc = depthDesc;

	pipelineStateStream.blend = alphaBlendDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(pipeline_state_stream), &pipelineStateStream
	};
	checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));

	//uint32 white = 0xFFFFFFFF;
	//commandList->loadTextureFromMemory(whiteTexture, &white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, texture_type_noncolor);

	registerMouseCallback(BIND(mouseCallback));
}

void debug_gui::resizeIndexBuffer(dx_command_list* commandList, uint32 numQuads)
{
	uint16* indices = new uint16[numQuads * 6];
	for (uint32 i = 0; i < numQuads; ++i)
	{
		indices[i * 6 + 0] = i * 4;
		indices[i * 6 + 1] = i * 4 + 1;
		indices[i * 6 + 2] = i * 4 + 2;
		indices[i * 6 + 3] = i * 4 + 1;
		indices[i * 6 + 4] = i * 4 + 3;
		indices[i * 6 + 5] = i * 4 + 2;
	}

	indexBuffer = commandList->createIndexBuffer(indices, numQuads * 6);
	delete[] indices;
}

void debug_gui::beginGroup(const char* name)
{
	text(name);
	++level;
}

void debug_gui::endGroup()
{
	assert(level > 0);
	--level;
}

void debug_gui::text(const char* format, va_list arg)
{
	char text[2048];
	uint32 N = vsnprintf(text, sizeof(text), format, arg);
	uint32 numCharacters = min<uint32>(N, sizeof(text));

	uint32 currentVertex = (uint32)currentVertices.size();
	currentVertices.resize(currentVertices.size() + numCharacters * 4);

	float cursorX = 3.f + level * 10.f;
	float scale = textHeight / font.height;
	float cursorY = yOffset * textHeight;

	uint32 color = 0xFFFFFFFF;

	uint32 skippedChars = 0;
	uint32 index = 0;
	while (text[index])
	{
		char c = text[index];
		if (c == ' ')
		{
			cursorX += font.spaceWidth * scale;
			++skippedChars;
			++index;
			continue;
		}

		if (c < FIRST_CODEPOINT || c > LAST_CODEPOINT)
		{
			c = '?';
		}

		uint32 glyphID = c - FIRST_CODEPOINT;

		font_glyph& glyph = font.glyphs[glyphID];

		float xStart = cursorX + glyph.offsetX * scale;
		float yStart = cursorY + glyph.offsetY * scale;

		float gWidth = glyph.width * scale;
		float gHeight = glyph.height * scale;

		currentVertices[currentVertex++] = { vec2(xStart, yStart), vec2(glyph.left, glyph.top), color };
		currentVertices[currentVertex++] = { vec2(xStart + gWidth, yStart), vec2(glyph.right, glyph.top), color };
		currentVertices[currentVertex++] = { vec2(xStart, yStart + gHeight), vec2(glyph.left, glyph.bottom), color };
		currentVertices[currentVertex++] = { vec2(xStart + gWidth, yStart + gHeight), vec2(glyph.right, glyph.bottom), color };

		char nextC = text[index + 1];
		cursorX += font.getAdvance(c, nextC) * scale;

		++index;
	}

	currentVertices.resize(currentVertices.size() - skippedChars);

	++yOffset;
}

void debug_gui::text(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	text(format, arg);
	va_end(arg);

}

void debug_gui::value(const char* name, float v)
{
	text("%s: %f", name, v);
}

void debug_gui::render(dx_command_list* commandList, const D3D12_VIEWPORT& viewport)
{
	assert(level == 0);

	if (currentVertices.size() > 0)
	{
		uint32 numIndices = (uint32)currentVertices.size() / 4 * 6;
		if (numIndices > indexBuffer.numIndices)
		{
			resizeIndexBuffer(commandList, numIndices);
		}

		D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(currentVertices.data(), (uint32)currentVertices.size());

		commandList->setPipelineState(pipelineState);
		commandList->setGraphicsRootSignature(rootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		vec2 invScreenDim = { 1.f / viewport.Width, 1.f / viewport.Height };
		commandList->setGraphics32BitConstants(0, invScreenDim);

		commandList->setShaderResourceView(1, 0, font.atlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


		commandList->setVertexBuffer(0, tmpVertexBuffer);
		commandList->setIndexBuffer(indexBuffer);

		commandList->drawIndexed(numIndices, 1, 0, 0, 0);
		
		currentVertices.clear();
	}

	yOffset = 1;
	level = 0;
}

bool debug_gui::mouseCallback(mouse_input_event event)
{
	return false;
}
