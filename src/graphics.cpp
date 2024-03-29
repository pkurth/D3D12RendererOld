#include "pch.h"
#include "common.h"
#include "graphics.h"

CD3DX12_BLEND_DESC alphaBlendDesc;
CD3DX12_BLEND_DESC additiveBlendDesc;
CD3DX12_BLEND_DESC reverseSubtractiveBlendDesc;
CD3DX12_BLEND_DESC minBlendDesc;
CD3DX12_BLEND_DESC maxBlendDesc;

CD3DX12_RASTERIZER_DESC defaultRasterizerDesc;
CD3DX12_RASTERIZER_DESC noBackfaceCullRasterizerDesc;

CD3DX12_DEPTH_STENCIL_DESC1 alwaysReplaceStencilDesc;
CD3DX12_DEPTH_STENCIL_DESC1 notEqualStencilDesc;
CD3DX12_DEPTH_STENCIL_DESC1 equalDepthDesc;

void initializeCommonGraphicsItems()
{
	alphaBlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	alphaBlendDesc.AlphaToCoverageEnable = false;
	alphaBlendDesc.IndependentBlendEnable = false;
	alphaBlendDesc.RenderTarget[0].BlendEnable = true;
	alphaBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	alphaBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	alphaBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

	additiveBlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	additiveBlendDesc.AlphaToCoverageEnable = false;
	additiveBlendDesc.IndependentBlendEnable = false;
	additiveBlendDesc.RenderTarget[0].BlendEnable = true;
	additiveBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	additiveBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	additiveBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	reverseSubtractiveBlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	reverseSubtractiveBlendDesc.AlphaToCoverageEnable = false;
	reverseSubtractiveBlendDesc.IndependentBlendEnable = false;
	reverseSubtractiveBlendDesc.RenderTarget[0].BlendEnable = true;
	reverseSubtractiveBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	reverseSubtractiveBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	reverseSubtractiveBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	minBlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	minBlendDesc.AlphaToCoverageEnable = false;
	minBlendDesc.IndependentBlendEnable = false;
	minBlendDesc.RenderTarget[0].BlendEnable = true;
	minBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_MIN;
	minBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	minBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	maxBlendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	maxBlendDesc.AlphaToCoverageEnable = false;
	maxBlendDesc.IndependentBlendEnable = false;
	maxBlendDesc.RenderTarget[0].BlendEnable = true;
	maxBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_MAX;
	maxBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	maxBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	defaultRasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	defaultRasterizerDesc.FrontCounterClockwise = TRUE; // Righthanded coordinate system.

	noBackfaceCullRasterizerDesc = defaultRasterizerDesc;
	noBackfaceCullRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // Disable backface culling.

	alwaysReplaceStencilDesc = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);
	alwaysReplaceStencilDesc.StencilEnable = true;
	alwaysReplaceStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	alwaysReplaceStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	alwaysReplaceStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	alwaysReplaceStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	alwaysReplaceStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	alwaysReplaceStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	equalDepthDesc = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);
	equalDepthDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	equalDepthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	notEqualStencilDesc = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT); 
	notEqualStencilDesc.StencilEnable = true;
	notEqualStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
}

CD3DX12_STATIC_SAMPLER_DESC staticPointClampSampler(uint32 shaderRegister)
{
	return CD3DX12_STATIC_SAMPLER_DESC(shaderRegister,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
};

CD3DX12_STATIC_SAMPLER_DESC staticLinearClampSampler(uint32 shaderRegister)
{
	return CD3DX12_STATIC_SAMPLER_DESC(shaderRegister,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
};

CD3DX12_STATIC_SAMPLER_DESC staticLinearWrapSampler(uint32 shaderRegister)
{
	return CD3DX12_STATIC_SAMPLER_DESC(shaderRegister,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}

CD3DX12_STATIC_SAMPLER_DESC staticShadowMapSampler(uint32 shaderRegister)
{
	return CD3DX12_STATIC_SAMPLER_DESC(shaderRegister,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER);
}

