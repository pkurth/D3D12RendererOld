#include "pch.h"
#include "resource_state_tracker.h"
#include "command_list.h"
#include "resource.h"

std::mutex dx_resource_state_tracker::globalMutex;
bool dx_resource_state_tracker::isLocked = false;
std::unordered_map<ID3D12Resource*, dx_resource_state_tracker::resource_state> dx_resource_state_tracker::globalResourceState;


void dx_resource_state_tracker::initialize()
{
}

void dx_resource_state_tracker::resourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
	if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
	{
		const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

		const auto it = finalResourceState.find(transitionBarrier.pResource);
		resource_state* resourceState;
		if (it != finalResourceState.end())
		{
			resourceState = &it->second;

			if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
				!resourceState->subresourceStates.empty())
			{
				// Transition all of the subresources if they are different than the StateAfter.
				for (uint32 i = 0; i < resourceState->subresourceStates.size(); ++i)
				{
					D3D12_RESOURCE_STATES subresourceState = resourceState->subresourceStates[i];

					if (transitionBarrier.StateAfter != subresourceState)
					{
						D3D12_RESOURCE_BARRIER newBarrier = barrier;
						newBarrier.Transition.Subresource = i;
						newBarrier.Transition.StateBefore = subresourceState;
						resourceBarriers.push_back(newBarrier);
					}
				}
			}
			else
			{
				D3D12_RESOURCE_STATES finalState = resourceState->getSubresourceState(transitionBarrier.Subresource);
				if (transitionBarrier.StateAfter != finalState)
				{
					D3D12_RESOURCE_BARRIER newBarrier = barrier;
					newBarrier.Transition.StateBefore = finalState;
					resourceBarriers.push_back(newBarrier);
				}
			}
		}
		else
		{
			pendingResourceBarriers.push_back(barrier);
			resourceState = &finalResourceState.insert({ transitionBarrier.pResource, resource_state() }).first->second;

			// Get subresource count from global list.
			std::lock_guard<std::mutex> lock(globalMutex);
			auto& global = globalResourceState[transitionBarrier.pResource];
			resourceState->initialize((uint32)global.subresourceStates.capacity());

			
		}

		resourceState->setSubresourceState(transitionBarrier.Subresource, transitionBarrier.StateAfter);
	}
	else
	{
		resourceBarriers.push_back(barrier);
	}
}

void dx_resource_state_tracker::transitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, uint32 subResource)
{
	if (resource)
	{
		resourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, stateAfter, subResource));
	}
}

void dx_resource_state_tracker::transitionResource(const dx_resource& resource, D3D12_RESOURCE_STATES stateAfter, uint32 subResource)
{
	transitionResource(resource.resource.Get(), stateAfter, subResource);
}

void dx_resource_state_tracker::uavBarrier(const dx_resource* resource)
{
	ID3D12Resource* d3d12resource = resource != nullptr ? resource->resource.Get() : nullptr;
	resourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(d3d12resource));
}

void dx_resource_state_tracker::aliasBarrier(const dx_resource* resourceBefore, const dx_resource* resourceAfter)
{
	ID3D12Resource* d3d12ResourceBefore = resourceBefore != nullptr ? resourceBefore->resource.Get() : nullptr;
	ID3D12Resource* d3d12ResourceAfter = resourceAfter != nullptr ? resourceAfter->resource.Get() : nullptr;

	resourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(d3d12ResourceBefore, d3d12ResourceAfter));
}

uint32 dx_resource_state_tracker::flushPendingResourceBarriers(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	assert(isLocked);

	// Resolve the pending resource barriers by checking the global state of the 
	// (sub)resources. Add barriers if the pending state and the global state do
	//  not match.
	std::vector<D3D12_RESOURCE_BARRIER> resourceBarriers;
	resourceBarriers.reserve(pendingResourceBarriers.size());

	for (auto pendingBarrier : pendingResourceBarriers)
	{
		if (pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)  // Only transition barriers should be pending...
		{
			auto pendingTransition = pendingBarrier.Transition;
			const auto& iter = globalResourceState.find(pendingTransition.pResource);
			if (iter != globalResourceState.end())
			{
				auto& resourceState = iter->second;
				if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
					!resourceState.subresourceStates.empty())
				{
					for (uint32 i = 0; i < resourceState.subresourceStates.size(); ++i)
					{
						D3D12_RESOURCE_STATES subresourceState = resourceState.subresourceStates[i];

						if (pendingTransition.StateAfter != subresourceState)
						{
							D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
							newBarrier.Transition.Subresource = i;
							newBarrier.Transition.StateBefore = subresourceState;
							resourceBarriers.push_back(newBarrier);
						}
					}
				}
				else
				{
					// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
					auto globalState = (iter->second).getSubresourceState(pendingTransition.Subresource);
					if (pendingTransition.StateAfter != globalState)
					{
						// Fix-up the before state based on current global state of the resource.
						pendingBarrier.Transition.StateBefore = globalState;
						resourceBarriers.push_back(pendingBarrier);
					}
				}
			}
		}
	}

	uint32 numBarriers = (uint32)resourceBarriers.size();
	if (numBarriers > 0)
	{
		commandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}

	pendingResourceBarriers.clear();

	return numBarriers;
}

void dx_resource_state_tracker::flushResourceBarriers(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	uint32 numBarriers = (uint32)resourceBarriers.size();
	if (numBarriers > 0)
	{
		commandList->ResourceBarrier(numBarriers, resourceBarriers.data());
		resourceBarriers.clear();
	}
}

void dx_resource_state_tracker::commitFinalResourceStates()
{
	assert(isLocked);

	for (const auto& resourceState : finalResourceState)
	{
		globalResourceState[resourceState.first] = resourceState.second;
	}

	finalResourceState.clear();
}

void dx_resource_state_tracker::reset()
{
	pendingResourceBarriers.clear();
	resourceBarriers.clear();
	finalResourceState.clear();
}

void dx_resource_state_tracker::lock()
{
	globalMutex.lock();
	isLocked = true;
}

void dx_resource_state_tracker::unlock()
{
	isLocked = false;
	globalMutex.unlock();
}

void dx_resource_state_tracker::addGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state, uint32 numSubResources)
{
	if (resource)
	{
		std::lock_guard<std::mutex> lock(globalMutex);

		auto& it = globalResourceState[resource];
		it.initialize(numSubResources, state);
		it.setSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
	}
}

void dx_resource_state_tracker::removeGlobalResourceState(ID3D12Resource* resource)
{
	if (resource)
	{
		std::lock_guard<std::mutex> lock(globalMutex);
		globalResourceState.erase(resource);
	}
}
