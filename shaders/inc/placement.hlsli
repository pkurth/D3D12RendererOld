#ifndef PLACEMENT_H
#define PLACEMENT_H



struct placement_submesh
{
	float4 aabbMin;
	float4 aabbMax;
};

struct placement_lod
{
	uint firstSubmesh;
	uint numSubmeshes;
};

struct placement_point
{
	float3 position;
	uint meshID;
	float3 normal;
	uint lod;
};

struct placement_mesh
{
	placement_lod lods[4];
	float3 lodDistances; // Last LOD has infinite distance.
	uint numLODs;
};



#endif
