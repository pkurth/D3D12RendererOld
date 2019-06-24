#pragma once

#include "common.h"

#include <d3d12.h>
#include <map>

struct resource_state
{

};

class dx_resource_state_tracker
{
public:
	void resourceBarrier(const D3D12_RESOURCE_BARRIER& barrier); 
	
	void transitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void UAVBarrier(ID3D12Resource* resource = nullptr);

	//uint32 FlushPendingResourceBarriers(CommandList& commandList);
};
