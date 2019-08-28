#pragma once

#define UNBOUNDED_DESCRIPTOR_RANGE -1

extern CD3DX12_BLEND_DESC alphaBlendDesc;
extern CD3DX12_BLEND_DESC additiveBlendDesc;

extern CD3DX12_RASTERIZER_DESC defaultRasterizerDesc;
extern CD3DX12_RASTERIZER_DESC noBackfaceCullRasterizerDesc;

extern CD3DX12_DEPTH_STENCIL_DESC1 alwaysReplaceStencilDesc;
extern CD3DX12_DEPTH_STENCIL_DESC1 notEqualStencilDesc;



void initializeCommonGraphicsItems();
CD3DX12_STATIC_SAMPLER_DESC staticPointClampSampler(uint32 shaderRegister);
CD3DX12_STATIC_SAMPLER_DESC staticLinearClampSampler(uint32 shaderRegister);
CD3DX12_STATIC_SAMPLER_DESC staticLinearWrapSampler(uint32 shaderRegister);
CD3DX12_STATIC_SAMPLER_DESC staticPointBorderComparisonSampler(uint32 shaderRegister);

#if defined(PROFILE) || defined(_DEBUG)
#define SET_NAME(obj, name) checkResult(obj->SetName(L##name));
#else
#define SET_NAME(obj, name)
#endif
