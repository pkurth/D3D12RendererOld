#pragma once

#include "common.h"

#include <dx/d3dx12.h>
#include <wrl.h> 
using namespace Microsoft::WRL;


struct dx_root_signature
{
	void initialize(ComPtr<ID3D12Device2> device, const D3D12_ROOT_SIGNATURE_DESC1& desc);
	
	inline ComPtr<ID3D12RootSignature> getD3D12RootSignature() const { return rootSignature; }
private:
	ComPtr<ID3D12RootSignature> rootSignature;
};

