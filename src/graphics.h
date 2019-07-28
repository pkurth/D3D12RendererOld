#pragma once

extern CD3DX12_BLEND_DESC alphaBlendDesc;
extern CD3DX12_BLEND_DESC additiveBlendDesc;

extern CD3DX12_RASTERIZER_DESC defaultRasterizerDesc;
extern CD3DX12_RASTERIZER_DESC noBackfaceCullRasterizerDesc;

extern CD3DX12_DEPTH_STENCIL_DESC1 alwaysReplaceStencilDesc;
extern CD3DX12_DEPTH_STENCIL_DESC1 notEqualStencilDesc;



void initializeCommonGraphicsItems();
