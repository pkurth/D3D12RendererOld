#define USE_ALBEDO_TEXTURE		(1 << 0)
#define USE_NORMAL_TEXTURE		(1 << 1)
#define USE_ROUGHNESS_TEXTURE	(1 << 2)
#define USE_METALLIC_TEXTURE	(1 << 3)
#define USE_AO_TEXTURE			(1 << 4)

struct material_cb
{
	uint textureID;
	uint usageFlags;

	float roughnessOverride;
	float metallicOverride;

	float4 albedoTint;

	uint drawID;
};
