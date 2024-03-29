#include "pch.h"
#include "debug_gui.h"
#include "error.h"
#include "graphics.h"

static bool pointInRectangle(vec2 p, vec2 topLeft, vec2 bottomRight)
{
	return p.x >= topLeft.x && p.y >= topLeft.y && p.x <= bottomRight.x && p.y <= bottomRight.y;
}

void debug_gui::initialize(ComPtr<ID3D12Device2> device, dx_command_list* commandList, D3D12_RT_FORMAT_ARRAY rtvFormats)
{
	this->device = device;
	resizeIndexBuffer(commandList, 2048);
	level = 0;
	font.initialize(commandList, "consola", 25, true);
	baselineTextHeight = font.height * 0.75f;
	textHeight = baselineTextHeight * guiScale;
	cursorY = 0.00001f;
	lastEventType = event_type_none;
	activeID = 0;
	tabAdvance = 0.f;
	numActiveTabs = 0;
	openTab = 0;
	firstTab = 0;
	openTabSeenThisFrame = false;

	mousePosition = vec2(0.f, 0.f);

	{
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
		fontRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = fontRootSignature.rootSignature.Get();
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
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&fontPipelineState)));
	}

	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/flat_2d_vs.cso", &vertexShaderBlob));
		ComPtr<ID3DBlob> pixelShaderBlob;
		checkResult(D3DReadFileToBlob(L"shaders/bin/flat_2d_ps.cso", &pixelShaderBlob));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
		rootParameters[0].InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Inv screen dimensions.

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
		rootSignatureDesc.Flags = rootSignatureFlags;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = arraysize(rootParameters);
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		shapeRootSignature.initialize(device, rootSignatureDesc);


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

		pipelineStateStream.rootSignature = shapeRootSignature.rootSignature.Get();
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
		checkResult(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&shapePipelineState)));
	}

	registerMouseButtonDownCallback(BIND(mouseDownCallback));
	registerMouseButtonUpCallback(BIND(mouseUpCallback));
	registerMouseMoveCallback(BIND(mouseMoveCallback));
	registerMouseScrollCallback(BIND(mouseScrollCallback));
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

	indexBuffer.initialize(device, indices, numQuads * 6, commandList);
	delete[] indices;
}

float debug_gui::getCursorX()
{
	if (cursorY == 0.f)
	{
		return 3.f + tabAdvance;
	}
	return 5.f + level * 10.f;
}

debug_gui::text_analysis debug_gui::analyzeText(const char* text, float size)
{
	float cursorX = getCursorX();
	float scale = textHeight / font.height * size;

	float cursorY = this->cursorY + textHeight * size;

	text_analysis result = { 0, vec2(FLT_MAX, FLT_MAX), vec2(-FLT_MAX, -FLT_MAX) };

	while (*text)
	{
		char c = *text++;
		if (c != ' ')
		{
			uint32 glyphID = c - FIRST_CODEPOINT;

			font_glyph& glyph = font.glyphs[glyphID];

			float xStart = cursorX + glyph.offsetX * scale;
			float yStart = cursorY + glyph.offsetY * scale;

			float gWidth = glyph.width * scale;
			float gHeight = glyph.height * scale;

			++result.numGlyphs;
			result.topLeft.x = min(result.topLeft.x, xStart);
			result.topLeft.y = min(result.topLeft.y, yStart);
			result.bottomRight.x = max(result.bottomRight.x, xStart + gWidth);
			result.bottomRight.y = max(result.bottomRight.y, yStart + gHeight);

			char nextC = *text;
			cursorX += font.getAdvance(c, nextC) * scale;
		}
		else
		{
			cursorX += font.spaceWidth * scale;
		}
	}

	return result;
}

#define MAX_TEXT_LENGTH 2048

void debug_gui::textInternalAt(float x, float y, const char* text, uint32 color, float size)
{
	currentFontVertices.reserve(currentFontVertices.size() + MAX_TEXT_LENGTH * 4);

	float cursorX = x;
	float scale = textHeight / font.height * size;

	float cursorY = y;

	while (*text)
	{
		char c = *text++;
		if (c == ' ')
		{
			cursorX += font.spaceWidth * scale;
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

		currentFontVertices.push_back({ vec2(xStart, yStart), vec2(glyph.left, glyph.top), color });
		currentFontVertices.push_back({ vec2(xStart + gWidth, yStart), vec2(glyph.right, glyph.top), color });
		currentFontVertices.push_back({ vec2(xStart, yStart + gHeight), vec2(glyph.left, glyph.bottom), color });
		currentFontVertices.push_back({ vec2(xStart + gWidth, yStart + gHeight), vec2(glyph.right, glyph.bottom), color });

		char nextC = *text;
		cursorX += font.getAdvance(c, nextC) * scale;
	}
}

void debug_gui::textInternal(const char* text, uint32 color, float size)
{
	if (numActiveTabs == 0)
	{
		assert(!"No open tab in GUI.");
	}

	float x = getCursorX();
	float y = this->cursorY + textHeight * size;
	cursorY = y;

	textInternalAt(x, y, text, color, size);
}

void debug_gui::textInternalF(const char* text, uint32 color, float size, ...)
{
	va_list arg;
	va_start(arg, size);
	textInternalV(text, arg, color, size);
	va_end(arg);
}

void debug_gui::textInternalV(const char* format, va_list arg, uint32 color, float size)
{
	char text[MAX_TEXT_LENGTH];
	vsnprintf(text, sizeof(text), format, arg);
	textInternal(text, color, size);
}

void debug_gui::textV(const char* format, va_list arg)
{
	textInternalV(format, arg);
}

void debug_gui::text(const char* text)
{
	textInternal(text);
}

void debug_gui::textF(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	textInternalV(format, arg);
	va_end(arg);
}

void debug_gui::textAt(float x, float y, uint32 color, const char* text)
{
	textInternalAt(x, y, text, color);
}

void debug_gui::textAtF(float x, float y, uint32 color, const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	textAtV(x, y, color, format, arg);
	va_end(arg);
}

void debug_gui::textAtV(float x, float y, uint32 color, const char* format, va_list arg)
{
	char text[MAX_TEXT_LENGTH];
	vsnprintf(text, sizeof(text), format, arg);
	textInternalAt(x, y, text, color);
}

void debug_gui::textAtMouse(const char* text)
{
	textInternalAt(mousePosition.x, mousePosition.y, text);
}

void debug_gui::textAtMouseF(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	textAtV(mousePosition.x, mousePosition.y, DEBUG_GUI_TEXT_COLOR, format, arg);
	va_end(arg);
}

void debug_gui::textAtMouseV(const char* format, va_list arg)
{
	char text[MAX_TEXT_LENGTH];
	vsnprintf(text, sizeof(text), format, arg);
	textInternalAt(mousePosition.x, mousePosition.y, text, DEBUG_GUI_TEXT_COLOR);
}

void debug_gui::value(const char* name, bool v)
{
	if (v) { textF("%s: true", name); }
	else { textF("%s: false", name); }
}

void debug_gui::value(const char* name, int32 v)
{
	textF("%s: %d", name, v);
}

void debug_gui::value(const char* name, uint32 v)
{
	textF("%s: %u", name, v);
}

void debug_gui::value(const char* name, float v)
{
	textF("%s: %f", name, v);
}

void debug_gui::quad(float left, float right, float top, float bottom, uint32 color)
{
	currentShapeVertices.push_back({ vec2(left, top), color });
	currentShapeVertices.push_back({ vec2(right, top), color });
	currentShapeVertices.push_back({ vec2(left, bottom), color });
	currentShapeVertices.push_back({ vec2(right, bottom), color });
}

bool debug_gui::quadButton(uint64 guid, float left, float right, float top, float bottom, uint32 color, const char* formatOnHover, ...)
{
	uint32 button = handleButtonPress(guid, vec2(left, top), vec2(right, bottom));
	if (button & 1)
	{
		color = (DEBUG_GUI_HOVERED_COLOR & 0xFFFFFF) | (color & 0xFF000000);
		
		if (formatOnHover)
		{
			va_list arg;
			va_start(arg, formatOnHover);
			textAtV(mousePosition.x, mousePosition.y, DEBUG_GUI_TEXT_COLOR, formatOnHover, arg);
			va_end(arg);
		}
	}
	quad(left, right, top, bottom, color);

	return button & 2;
}

bool debug_gui::quadHover(float left, float right, float top, float bottom, uint32 color)
{
	bool result = false;
	if (pointInRectangle(mousePosition, vec2(left, top), vec2(right, bottom)))
	{
		result = true;
		color = (DEBUG_GUI_HOVERED_COLOR & 0xFFFFFF) | (color & 0xFF000000);
	}
	quad(left, right, top, bottom, color);
	return result;
}

float debug_gui::quadScroll(float left, float right, float top, float bottom, uint32 color)
{
	float result = 0.f;
	if (pointInRectangle(mousePosition, vec2(left, top), vec2(right, bottom)))
	{
		result = mouseScroll;
		color = (DEBUG_GUI_HOVERED_COLOR & 0xFFFFFF) | (color & 0xFF000000);
	}
	quad(left, right, top, bottom, color);
	return result;
}

debug_gui_interaction debug_gui::interactableQuad(uint64 guid, float left, float right, float top, float bottom, uint32 color)
{
	debug_gui_interaction result = {};

	uint32 button = handleButtonPress(guid, vec2(left, top), vec2(right, bottom));
	if (button & 1)
	{
		color = (DEBUG_GUI_HOVERED_COLOR & 0xFFFFFF) | (color & 0xFF000000);
		result.hover = true;
		result.scroll = mouseScroll;

		if (lastEventType == event_type_down)
		{
			result.downEvent = true;
		}
		if (lastEventType == event_type_up)
		{
			result.upEvent = true;
		}
	}
	quad(left, right, top, bottom, color);

	result.click = button & 2;
	return result;
}

void debug_gui::render(dx_command_list* commandList, const D3D12_VIEWPORT& viewport)
{
	assert(abs(level) < 0.01f);

	uint32 numFontIndices = (uint32)currentFontVertices.size() / 4 * 6;
	uint32 numShapeIndices = (uint32)currentShapeVertices.size() / 4 * 6;

	if (numFontIndices > indexBuffer.numIndices || numShapeIndices >indexBuffer.numIndices)
	{
		resizeIndexBuffer(commandList, max(numFontIndices, numShapeIndices));
	}

	vec2 invScreenDim = { 1.f / viewport.Width, 1.f / viewport.Height };

	if (currentShapeVertices.size() > 0)
	{
		D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(currentShapeVertices.data(), (uint32)currentShapeVertices.size());

		commandList->setPipelineState(shapePipelineState);
		commandList->setGraphicsRootSignature(shapeRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->setGraphics32BitConstants(0, invScreenDim);

		commandList->setVertexBuffer(0, tmpVertexBuffer);
		commandList->setIndexBuffer(indexBuffer);

		commandList->drawIndexed(numShapeIndices, 1, 0, 0, 0);

		currentShapeVertices.clear();
	}

	if (currentFontVertices.size() > 0)
	{
		D3D12_VERTEX_BUFFER_VIEW tmpVertexBuffer = commandList->createDynamicVertexBuffer(currentFontVertices.data(), (uint32)currentFontVertices.size());

		commandList->setPipelineState(fontPipelineState);
		commandList->setGraphicsRootSignature(fontRootSignature);

		commandList->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->setGraphics32BitConstants(0, invScreenDim);

		commandList->setShaderResourceView(1, 0, font.atlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


		commandList->setVertexBuffer(0, tmpVertexBuffer);
		commandList->setIndexBuffer(indexBuffer);

		commandList->drawIndexed(numFontIndices, 1, 0, 0, 0);
		
		currentFontVertices.clear();
	}

	cursorY = 0.00001f;
	level = 0;
	tabAdvance = 0.f;
	if (!openTabSeenThisFrame)
	{
		openTab = firstTab; // This happens if a tab was closed.
	}
	numActiveTabs = 0;
	firstTab = 0;
	openTabSeenThisFrame = false;

	if (lastEventType == event_type_up)
	{
		activeID = 0;
	}

	lastEventType = event_type_none;
	mouseScroll = 0.f;
	allTabsSeenThisFrame.clear();
	textHeight = baselineTextHeight * guiScale;
}

bool debug_gui::mouseDownCallback(mouse_button_event event)
{
	mousePosition = vec2((float)event.x, (float)event.y);
	if (event.button == mouse_left)
	{
		lastEventType = event_type_down;
		mouseDown = true;
	}
	return false;
}

bool debug_gui::mouseUpCallback(mouse_button_event event)
{
	mousePosition = vec2((float)event.x, (float)event.y);
	if (event.button == mouse_left)
	{
		lastEventType = event_type_up;
		mouseDown = false;
	}
	return false;
}

bool debug_gui::mouseMoveCallback(mouse_move_event event)
{
	mousePosition = vec2((float)event.x, (float)event.y);
	return false;
}

bool debug_gui::mouseScrollCallback(mouse_scroll_event event)
{
	mouseScroll += event.scroll;
	return false;
}

uint64 debug_gui::hashLabel(const char* name)
{
	return std::hash<const char*>()(name);
}

// Returns bitmask. 1 is set if hovered, 2 is set if pressed.
uint32 debug_gui::handleButtonPress(uint64 id, vec2 topLeft, vec2 bottomRight)
{
	uint32 result = 0;
	if (pointInRectangle(mousePosition, topLeft, bottomRight))
	{
		result |= 1;

		if (lastEventType == event_type_down)
		{
			activeID = id;
		}
		else if (lastEventType == event_type_up)
		{
			if (activeID == id)
			{
				result |= 2;
			}
			activeID = 0;
		}
	}
	return result;
}

uint32 debug_gui::handleButtonPress(uint64 id, const char* name, float size)
{
	text_analysis info = analyzeText(name, size);

	uint32 result = handleButtonPress(id, info.topLeft, info.bottomRight);

	uint32 right = (uint32)info.bottomRight.x;
	result |= right << 2;

	return result;
}

bool debug_gui::buttonInternal(uint64 id, const char* name, uint32 color, float size, bool isTab)
{
	uint32 button = handleButtonPress(id, name, size);
	if (button & 1)
	{
		color = (DEBUG_GUI_HOVERED_COLOR & 0xFFFFFF) | (color & 0xFF000000);
	}
	textInternal(name, color, size);

	if (isTab)
	{
		uint32 right = button >> 2;
		tabAdvance = right + 100.f * guiScale;
	}

	return button & 2;
}

bool debug_gui::buttonInternalF(uint64 id, const char* name, uint32 color, float size, ...)
{
	va_list arg;
	va_start(arg, size); // Must be last argument before '...' .
	char text[MAX_TEXT_LENGTH];
	vsnprintf(text, sizeof(text), name, arg);
	va_end(arg);

	uint32 button = handleButtonPress(id, text, size);
	if (button & 1)
	{
		color = (DEBUG_GUI_HOVERED_COLOR & 0xFFFFFF) | (color & 0xFF000000);
	}
	textInternal(text, color, size);
	return button & 2;
}

bool debug_gui::toggle(const char* name, bool& v)
{
	if (buttonInternalF(hashLabel(name), "%s: %s", DEBUG_GUI_TOGGLE_COLOR, 1.f, name, v ? "on" : "off"))
	{
		v = !v;
		return true;
	}
	return false;
}

bool debug_gui::button(const char* name)
{
	return buttonInternal(hashLabel(name), name, DEBUG_GUI_BUTTON_COLOR);
}

bool debug_gui::slider(const char* name, float& v, float min, float max)
{
	float left = getCursorX();
	float right = left + 200.f * guiScale;
	float y = cursorY + textHeight * 0.75f;
	float top = y - 1.5f;
	float bottom = y + 1.5f;
	quad(left, right, top, bottom, DEBUG_GUI_TEXT_COLOR);

	v = clamp(v, min, max);
	float x = lerp(left, right, inverseLerp(min, max, v));
	
	float cursorLeft = x - 2;
	float cursorRight = x + 2;
	float cursorTop = y - textHeight * 0.4f;
	float cursorBottom = y + textHeight * 0.4f;

	uint64 id = hashLabel(name);

	bool result = false;

	uint32 color = DEBUG_GUI_BUTTON_COLOR;

	if (pointInRectangle(mousePosition, vec2(cursorLeft, cursorTop), vec2(cursorRight, cursorBottom)))
	{
		if (lastEventType == event_type_down)
		{
			activeID = id;
		}

		color = DEBUG_GUI_HOVERED_COLOR;
	}

	if (activeID == id)
	{
		color = DEBUG_GUI_HOVERED_COLOR;

		float mouseX = mousePosition.x;
		float t = clamp01(inverseLerp(left, right, mouseX));
		v = lerp(min, max, t);
		x = lerp(left, right, t);

		cursorLeft = x - 2;
		cursorRight = x + 2;
		cursorTop = y - textHeight * 0.4f;
		cursorBottom = y + textHeight * 0.4f;

		result = true;
	}

	quad(cursorLeft, cursorRight, cursorTop, cursorBottom, color);

	cursorY += textHeight;

	textAtF(right + 4.f, cursorY, DEBUG_GUI_TEXT_COLOR, "%s: %.3f", name, v);

	return result;
}

bool debug_gui::multislider(const char* name, float* values, uint32 numValues, float min, float max, float minDistance)
{
	float left = getCursorX();
	float right = left + 200.f * guiScale;
	float y = cursorY + textHeight * 0.75f;
	float top = y - 1.5f;
	float bottom = y + 1.5f;
	quad(left, right, top, bottom, DEBUG_GUI_TEXT_COLOR);

	uint64 id = hashLabel(name);

	bool result = false;

	assert((numValues - 1) * minDistance <= max - min);

	uint32 hovered = -1;

	for (uint32 i = 0; i < numValues; ++i)
	{
		float localMin = min + i * minDistance;
		float localMax = max - (numValues - i - 1) * minDistance;

		float& v = values[i];
		v = clamp(v, localMin, localMax);
		float x = lerp(left, right, inverseLerp(min, max, v));

		float cursorLeft = x - 2;
		float cursorRight = x + 2;
		float cursorTop = y - textHeight * 0.4f;
		float cursorBottom = y + textHeight * 0.4f;

		if (pointInRectangle(mousePosition, vec2(cursorLeft, cursorTop), vec2(cursorRight, cursorBottom)))
		{
			if (lastEventType == event_type_down)
			{
				activeID = id + i;
			}
			hovered = i;
		}

		if (activeID == id + i)
		{
			float mouseX = mousePosition.x;
			float t = clamp01(inverseLerp(left, right, mouseX));
			v = clamp(lerp(min, max, t), localMin, localMax);
			t = inverseLerp(min, max, v);
			x = lerp(left, right, t);

			for (int j = (int)i - 1; j > -1; --j)
			{
				float distance = values[j + 1] - values[j];
				if (distance < minDistance)
				{
					values[j] = values[j + 1] - minDistance;
				}
			}
			for (uint32 j = i + 1; j < numValues; ++j)
			{
				float distance = values[j] - values[j - 1];
				if (distance < minDistance)
				{
					values[j] = values[j - 1] + minDistance;
				}
			}

			result = true;
		}
	}

	for (uint32 i = 0; i < numValues; ++i)
	{
		float v = values[i];
		float x = lerp(left, right, inverseLerp(min, max, v));

		float cursorLeft = x - 2;
		float cursorRight = x + 2;
		float cursorTop = y - textHeight * 0.4f;
		float cursorBottom = y + textHeight * 0.4f;
		
		uint32 color = DEBUG_GUI_BUTTON_COLOR;
		if (activeID == id + i || i == hovered)
		{
			color = DEBUG_GUI_HOVERED_COLOR;
		}
		quad(cursorLeft, cursorRight, cursorTop, cursorBottom, color);
	}

	cursorY += textHeight;

	char buffer[1024];
	int index = sprintf(buffer, "%s:", name);
	for (uint32 i = 0; i < numValues; ++i)
	{
		index += sprintf(buffer + index, " %.3f,", values[i]);
	}
	buffer[index - 1] = 0;

	textAt(right + 4.f, cursorY, DEBUG_GUI_TEXT_COLOR, buffer);

	return result;
}

bool debug_gui::radio(const char* name, const char** values, uint32 numValues, uint32& currentValue)
{
	uint32 originalValue = currentValue;

	currentValue = clamp(currentValue, 0u, numValues - 1);

	text(name);
	++level;

	for (uint32 i = 0; i < numValues; ++i)
	{
		uint64 id = hashLabel(values[i]);

		uint32 color = DEBUG_GUI_TEXT_COLOR;
		if (i == currentValue)
		{
			color = DEBUG_GUI_TOGGLE_COLOR;
		}

		if (buttonInternal(id, values[i], color))
		{
			currentValue = i;
		}
	}

	--level;

	return originalValue != currentValue;
}

void debug_gui::graph(float* values, uint32 numValues, float min, float max)
{
	assert(numValues > 1);

	const float graphSize = 200.f * guiScale;

	float stepSize = 1.f / (numValues - 1);

	for (uint32 i = 0; i < numValues - 1; ++i)
	{
		float x0 = graphSize * i * stepSize;
		float y0 = remap(values[i], min, max, 0.f, graphSize);
		float x1 = graphSize * (i + 1) * stepSize;
		float y1 = remap(values[i + 1], min, max, 0.f, graphSize);

		y0 = graphSize - y0 + cursorY;
		y1 = graphSize - y1 + cursorY;

		float cursorX = getCursorX();
		x0 += cursorX;
		x1 += cursorX;

		float thickness = 1.5f;

		vec2 p0(x0, y0);
		vec2 p1(x1, y1);
		vec2 d(x1 - x0, y1 - y0);

		vec2 n(-d.y, d.x);
		float l = sqrt(n.x * n.x + n.y * n.y); // Must be > 0.
		n.x *= thickness / l;
		n.y *= thickness / l;

		uint32 color = 0xFFFFFFFF;
		currentShapeVertices.push_back({ p0 - n, color });
		currentShapeVertices.push_back({ p1 - n, color });
		currentShapeVertices.push_back({ p0 + n, color });
		currentShapeVertices.push_back({ p1 + n, color });
	}

	cursorY += graphSize;
}

void debug_gui::graph(float(* eval_func)(void* data, float normX), uint32 numValues, float min, float max, void* data, 
	void(* manip_func)(void* data, debug_gui& gui))
{
	float origCursorY = cursorY;

	if (manip_func)
	{
		level += 21 * guiScale;
		manip_func(data, *this);
		level -= 21 * guiScale;
	}

	assert(numValues > 1);

	const float graphSize = 200.f * guiScale;

	float stepSize = 1.f / (numValues - 1);

	float cursorX = getCursorX();

	float x0 = 0.f;
	float y0 = remap(eval_func(data, 0.f), min, max, 0.f, graphSize);

	x0 += cursorX;
	y0 = graphSize - y0 + origCursorY;

	for (uint32 i = 0; i < numValues - 1; ++i)
	{
		float x1 = graphSize * (i + 1) * stepSize;
		float y1 = remap(eval_func(data, (i + 1) * stepSize), min, max, 0.f, graphSize);

		y1 = graphSize - y1 + origCursorY;
		x1 += cursorX;

		float thickness = 1.5f;

		vec2 p0(x0, y0);
		vec2 p1(x1, y1);
		vec2 d(x1 - x0, y1 - y0);

		vec2 n(-d.y, d.x);
		float l = sqrt(n.x * n.x + n.y * n.y); // Must be > 0.
		n.x *= thickness / l;
		n.y *= thickness / l;

		uint32 color = 0xFFFFFFFF;
		currentShapeVertices.push_back({ p0 - n, color });
		currentShapeVertices.push_back({ p1 - n, color });
		currentShapeVertices.push_back({ p0 + n, color });
		currentShapeVertices.push_back({ p1 + n, color });

		x0 = x1;
		y0 = y1;
	}

	if (cursorY < origCursorY + graphSize)
	{
		cursorY = origCursorY + graphSize;
	}
}

bool debug_gui::beginGroupInternal(const char* name, bool& isOpen)
{
	const float scale = 1.1f;

	if (buttonInternal(hashLabel(name), name, DEBUG_GUI_GROUP_COLOR, scale))
	{
		isOpen = !isOpen;
	}

	if (isOpen)
	{
		++level;
	}
	return isOpen;
}

void debug_gui::endGroupInternal()
{
	assert(level > 0);
	--level;
}

bool debug_gui::tab(const char* name)
{
	assert(abs(level) < 0.01f);

	const float scale = 1.5f;

	uint64 id = hashLabel(name);
	if (id == openTab && openTabSeenThisFrame)
	{
		// We have encountered this tab this frame already, so restore it.
		return true;
	}
	else
	{
		// Open new tab.

		for (uint64 i : allTabsSeenThisFrame)
		{
			if (id == i)
			{
				return false;
			}
		}

		allTabsSeenThisFrame.push_back(id);

		float cursorRestore = cursorY;
		cursorY = 0.f;

		if (numActiveTabs == 0)
		{
			firstTab = id;
		}

		++numActiveTabs;

		uint32 color = DEBUG_GUI_TAB_COLOR;
		if (openTab != id)
		{
			color = DEBUG_GUI_UNSELECTED_TAB_COLOR;
		}

		if (buttonInternal(id, name, color, scale, true))
		{
			openTab = id;
		}

		if (openTab == id)
		{
			openTabSeenThisFrame = true;
			return true;
		}

		cursorY = cursorRestore;
		return false;
	}
}

