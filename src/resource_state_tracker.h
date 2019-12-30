#pragma once

#include "common.h"


class dx_command_list;
struct dx_resource;

class dx_resource_state_tracker
{
public:
	void initialize();

	void resourceBarrier(const D3D12_RESOURCE_BARRIER& barrier); 
	
	void transitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, uint32 subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void transitionResource(const dx_resource& resource, D3D12_RESOURCE_STATES stateAfter, uint32 subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void uavBarrier(const dx_resource* resource = nullptr);
	void aliasBarrier(const dx_resource* resourceBefore = nullptr, const dx_resource* resourceAfter = nullptr);

	uint32 flushPendingResourceBarriers(ComPtr<ID3D12GraphicsCommandList2> commandList);
	void flushResourceBarriers(ComPtr<ID3D12GraphicsCommandList2> commandList);
	void commitFinalResourceStates();
	void reset();

	static void lock();
	static void unlock();

	static void addGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state, uint32 numSubResources);
	static void removeGlobalResourceState(ID3D12Resource* resource);

	static D3D12_RESOURCE_STATES getLastKnownGlobalState(ID3D12Resource* resource)
	{
		std::lock_guard<std::mutex> lock(globalMutex);
		return globalResourceState[resource].state;
	}

	D3D12_RESOURCE_STATES getLastKnownLocalState(ID3D12Resource* resource)
	{
		return finalResourceState[resource].state;
	}

private:
	struct resource_state
	{
		void initialize(uint32 numSubResources, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON)
		{
			this->state = state;
			if (numSubResources > 1)
			{
				subresourceStates.reserve(numSubResources);
			}
		}

		void setSubresourceState(uint32 subresource, D3D12_RESOURCE_STATES state)
		{
			if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES || subresourceStates.capacity() == 0)
			{
				this->state = state;
				subresourceStates.clear();
			}
			else
			{
				assert(subresource < subresourceStates.capacity());

				if (subresourceStates.empty())
				{
					subresourceStates.resize(subresourceStates.capacity(), this->state);
				}
				subresourceStates[subresource] = state;
			}
		}

		D3D12_RESOURCE_STATES getSubresourceState(uint32 subresource) const
		{
			D3D12_RESOURCE_STATES state = this->state;
			if (!subresourceStates.empty())
			{
				assert(subresource < subresourceStates.size());
				state = subresourceStates[subresource];
			}
			return state;
		}

		D3D12_RESOURCE_STATES state;
		std::vector<D3D12_RESOURCE_STATES> subresourceStates;
	};

	std::vector<D3D12_RESOURCE_BARRIER> resourceBarriers;
	std::vector<D3D12_RESOURCE_BARRIER> pendingResourceBarriers;

	std::unordered_map<ID3D12Resource*, resource_state> finalResourceState;

	static std::unordered_map<ID3D12Resource*, resource_state> globalResourceState;
	static std::mutex globalMutex;
	static bool isLocked;
};
