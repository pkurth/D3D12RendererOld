#include "pch.h"
#include "root_signature.h"
#include "error.h"

void dx_root_signature::initialize(ComPtr<ID3D12Device2> device, const D3D12_ROOT_SIGNATURE_DESC1& desc, bool parse)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(desc.NumParameters, desc.pParameters, desc.NumStaticSamplers, desc.pStaticSamplers, desc.Flags);

	// Serialize the root signature.
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	checkResult(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	// Create the root signature.
	checkResult(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	descriptorTableBitMask = 0;
	samplerTableBitMask = 0;

	D3D12_ROOT_PARAMETER1* parameters = new D3D12_ROOT_PARAMETER1[desc.NumParameters];
	D3D12_STATIC_SAMPLER_DESC* staticSamplers = new D3D12_STATIC_SAMPLER_DESC[desc.NumStaticSamplers];

	memcpy(parameters, desc.pParameters, sizeof(D3D12_ROOT_PARAMETER1) * desc.NumParameters);
	memcpy(staticSamplers, desc.pStaticSamplers, sizeof(D3D12_ROOT_PARAMETER1) * desc.NumStaticSamplers);

	for (uint32 i = 0; i < desc.NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = desc.pParameters[i];

		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			uint32 numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;

			D3D12_DESCRIPTOR_RANGE1* descriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;
			memcpy(descriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges, sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);
			parameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
			parameters[i].DescriptorTable.pDescriptorRanges = descriptorRanges;

			if (parse)
			{
				if (numDescriptorRanges > 0)
				{
					switch (rootParameter.DescriptorTable.pDescriptorRanges[0].RangeType)
					{
					case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
					case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
					case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
						if (rootParameter.DescriptorTable.pDescriptorRanges[0].NumDescriptors != -1) // Only if bounded.
						{
							descriptorTableBitMask |= (1 << i);
						}
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
						samplerTableBitMask |= (1 << i);
						break;
					}
				}
			}

			for (uint32 j = 0; j < numDescriptorRanges; ++j)
			{
				numDescriptorsPerTable[i] += rootParameter.DescriptorTable.pDescriptorRanges[j].NumDescriptors;
			}
		}
	}

	this->desc = desc;
	this->desc.pParameters = parameters;
	this->desc.pStaticSamplers = staticSamplers;
}

void dx_root_signature::shutdown()
{
	for (uint32 i = 0; i < desc.NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = desc.pParameters[i];

		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			uint32 numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
			if (numDescriptorRanges != -1)
			{
				delete[] rootParameter.DescriptorTable.pDescriptorRanges;
			}
		}
	}
	if (desc.NumParameters > 0)
	{
		delete[] desc.pParameters;
	}
	if (desc.NumStaticSamplers)
	{
		delete[] desc.pStaticSamplers;
	}
	desc = { };
}

uint32 dx_root_signature::getDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const
{
	uint32 descriptorTableBitMask = 0;
	switch (descriptorHeapType)
	{
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		descriptorTableBitMask = this->descriptorTableBitMask;
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
		descriptorTableBitMask = this->samplerTableBitMask;
		break;
	}

	return descriptorTableBitMask;
}

