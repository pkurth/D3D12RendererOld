#pragma once

#include "common.h"
#include "math.h"

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct vertex_3P
{
	vec3 position;
};

struct vertex_3PUN
{
	vec3 position;
	vec2 uv;
	vec3 normal;
};

struct vertex_3PUNT
{
	vec3 position;
	vec2 uv;
	vec3 normal;
	vec3 tangent;
};

struct indexed_triangle16
{
	uint16 a, b, c;
};

struct indexed_triangle32
{
	uint32 a, b, c;
};

struct indexed_line16
{
	uint16 a, b;
};

struct indexed_line32
{
	uint32 a, b, c;
};

defineHasMember(position);
defineHasMember(normal);
defineHasMember(tangent);
defineHasMember(uv);

template <typename vertex_t>
void setPosition(vertex_t& v, vec3 p)
{
	if constexpr (hasMember(vertex_t, position))
	{
		v.position = p;
	}
}

template <typename vertex_t>
void setNormal(vertex_t& v, vec3 n)
{
	if constexpr (hasMember(vertex_t, normal))
	{
		v.normal = n;
	}
}

template <typename vertex_t>
void setTangent(vertex_t& v, vec3 n)
{
	if constexpr (hasMember(vertex_t, tangent))
	{
		v.tangent = n;
	}
}

template <typename vertex_t>
void setUV(vertex_t& v, vec2 uv)
{
	if constexpr (hasMember(vertex_t, uv))
	{
		v.uv = uv;
	}
}

struct submesh_info
{
	uint32 firstTriangle;
	uint32 numTriangles;
	uint32 baseVertex;

	uint32 materialIndex;
};

struct submesh_material_info
{
	std::string albedoName;
	std::string normalName;
	std::string roughnessName;
	std::string metallicName;
};

template <typename vertex_t>
struct cpu_triangle_mesh
{
	std::vector<vertex_t> vertices;
	std::vector<indexed_triangle16> triangles;

	submesh_info pushQuad(float radius = 1.f);
	submesh_info pushCube(float radius = 1.f, bool invertWindingOrder = false);
	submesh_info pushSphere(uint16 slices, uint16 rows, float radius = 1.f);
	submesh_info pushCapsule(uint16 slices, uint16 rows, float height, float radius = 1.f);

	std::pair<std::vector<submesh_info>, std::vector<submesh_material_info>> pushFromFile(const std::string& filename);

private:
	submesh_info loadAssimpMesh(const aiMesh* mesh);
	submesh_material_info loadAssimpMaterial(const aiMaterial* material, const fs::path& parent);

	std::vector<submesh_info> splitSubmesh(submesh_info submesh, float maxDim, uint32 maxNumTriangles);
};

template<typename vertex_t>
inline std::pair<std::vector<submesh_info>, std::vector<submesh_material_info>> cpu_triangle_mesh<vertex_t>::pushFromFile(const std::string& filename)
{
	fs::path path(filename);
	assert(fs::exists(path));

	fs::path exportPath = path;
	exportPath.replace_extension("assbin");

	fs::path parent = path.parent_path();

	Assimp::Importer importer;
	const aiScene* scene;

	// Check if a preprocessed file exists.
	if (fs::exists(exportPath) && fs::is_regular_file(exportPath))
	{
		scene = importer.ReadFile(exportPath.string(), 0);
	}
	else
	{
		// File has not been preprocessed yet. Import and processes the file.
		importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX);

		//importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 2000);

		uint32 importFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
		uint32 exportFlags = 0;

		scene = importer.ReadFile(path.string(), importFlags);
		if (!scene)
		{
			std::cerr << importer.GetErrorString() << std::endl;
		}

		if (scene)
		{
			// Export the preprocessed scene file for faster loading next time.
			Assimp::Exporter exporter;
			exporter.Export(scene, "assbin", exportPath.string(), exportFlags);
		}
	}

	std::vector<submesh_info> submeshes;
	std::vector<submesh_material_info> submeshMaterials;

	if (scene)
	{
		for (uint32 i = 0; i < scene->mNumMeshes; ++i)
		{
			submesh_info sub = loadAssimpMesh(scene->mMeshes[i]);
#if 1
			submeshes.push_back(sub);
#else
			append(submeshes, splitSubmesh(sub, 500000.f, 2000));
#endif
		}

		submeshMaterials.resize(scene->mNumMaterials);
		for (uint32 i = 0; i < scene->mNumMaterials; ++i)
		{
			submeshMaterials[i] = loadAssimpMaterial(scene->mMaterials[i], parent);
		}
	}

	return { submeshes, submeshMaterials };
}

template<typename vertex_t>
inline submesh_info cpu_triangle_mesh<vertex_t>::loadAssimpMesh(const aiMesh* mesh)
{
	uint32 baseVertex = (uint32)vertices.size();
	uint32 firstTriangle = (uint32)triangles.size();
	uint32 numTriangles = mesh->mNumFaces;

	assert(mesh->mNumVertices <= UINT16_MAX);

	vertices.resize(vertices.size() + mesh->mNumVertices);
	triangles.resize(triangles.size() + mesh->mNumFaces);

	vec3 position(0.f, 0.f, 0.f);
	vec3 normal(0.f, 0.f, 0.f);
	vec3 tangent(0.f, 0.f, 0.f);
	vec2 uv(0.f, 0.f);

	for (uint32 i = 0; i < mesh->mNumVertices; ++i)
	{
		if (mesh->HasPositions())
		{
			position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		}
		if (mesh->HasNormals())
		{
			normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
		if (mesh->HasTangentsAndBitangents())
		{
			tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
		}
		if (mesh->HasTextureCoords(0))
		{
			uv = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}

		setPosition(vertices[i + baseVertex], position);
		setNormal(vertices[i + baseVertex], normal);
		setTangent(vertices[i + baseVertex], tangent);
		setUV(vertices[i + baseVertex], uv);
	}


	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		triangles[i + firstTriangle].a = face.mIndices[0];
		triangles[i + firstTriangle].b = face.mIndices[1];
		triangles[i + firstTriangle].c = face.mIndices[2];
	}

	return submesh_info{ firstTriangle, numTriangles, baseVertex, mesh->mMaterialIndex };
}

template<typename vertex_t>
inline submesh_material_info cpu_triangle_mesh<vertex_t>::loadAssimpMaterial(const aiMaterial* material, const fs::path& parent)
{
	aiString diffuse, normal, roughness, metallic;
	aiReturn hasDiffuse = material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse);
	aiReturn hasNormal = material->GetTexture(aiTextureType_HEIGHT, 0, &normal);
	aiReturn hasRoughness = material->GetTexture(aiTextureType_SHININESS, 0, &roughness);
	aiReturn hasMetallic = material->GetTexture(aiTextureType_AMBIENT, 0, &metallic);

	submesh_material_info result;
	if (hasDiffuse == aiReturn_SUCCESS) { result.albedoName = (parent / fs::path(diffuse.C_Str())).string(); }
	if (hasNormal == aiReturn_SUCCESS) { result.normalName = (parent / fs::path(normal.C_Str())).string(); }
	if (hasRoughness == aiReturn_SUCCESS) { result.roughnessName = (parent / fs::path(roughness.C_Str())).string(); }
	if (hasMetallic == aiReturn_SUCCESS) { result.metallicName = (parent / fs::path(metallic.C_Str())).string(); }

	return result;
}

template<typename vertex_t>
inline submesh_info cpu_triangle_mesh<vertex_t>::pushQuad(float radius)
{
	vertex_3PUN vertices[] = {
		{ { -radius, -radius, 0.f }, { 0.f, 0.f }, { 0.f, 0.f, 1.f } },
		{ { radius, -radius, 0.f }, { 1.f, 0.f }, { 0.f, 0.f, 1.f } },
		{ { -radius, radius, 0.f }, { 0.f, 1.f }, { 0.f, 0.f, 1.f } },
		{ { radius, radius, 0.f }, { 1.f, 1.f }, { 0.f, 0.f, 1.f } },
	};

	indexed_triangle16 triangles[] = {
		{ 0, 1, 2 },
		{ 1, 3, 2 }
	};

	uint32 baseVertex = (uint32)this->vertices.size();
	uint32 firstTriangle = (uint32)this->triangles.size();
	uint32 numTriangles = arraysize(triangles);

	this->vertices.resize(this->vertices.size() + arraysize(vertices));
	this->triangles.resize(this->vertices.size() + arraysize(triangles));

	for (uint32 i = 0; i < arraysize(vertices); ++i)
	{
		setPosition(this->vertices[i + baseVertex], vertices[i].position);
		setUV(this->vertices[i + baseVertex], vertices[i].uv);
		setNormal(this->vertices[i + baseVertex], vertices[i].normal);
	}

	memcpy(this->triangles.data() + firstTriangle, triangles, sizeof(triangles));

	return submesh_info{ firstTriangle, numTriangles, baseVertex };
}

template<typename vertex_t>
inline submesh_info cpu_triangle_mesh<vertex_t>::pushCube(float radius, bool invertWindingOrder)
{
	if constexpr (hasMember(vertex_t, position) && !hasMember(vertex_t, uv) && !hasMember(vertex_t, normal))
	{
		vertex_3P vertices[8];
		indexed_triangle16 triangles[12];

		vertices[0] = { vec3(-radius, -radius, radius) };  // 0
		vertices[1] = { vec3(radius, -radius, radius) };   // x
		vertices[2] = { vec3(-radius, radius, radius) };   // y
		vertices[3] = { vec3(radius, radius, radius) };	   // xy
		vertices[4] = { vec3(-radius, -radius, -radius) }; // z
		vertices[5] = { vec3(radius, -radius, -radius) };  // xz
		vertices[6] = { vec3(-radius, radius, -radius) };  // yz
		vertices[7] = { vec3(radius, radius, -radius) };   // xyz

		triangles[0] = { 0, 1, 2 };
		triangles[1] = { 1, 3, 2 };

		triangles[2] = { 1, 5, 3 };
		triangles[3] = { 5, 7, 3 };

		triangles[4] = { 5, 4, 7 };
		triangles[5] = { 4, 6, 7 };

		triangles[6] = { 4, 0, 6 };
		triangles[7] = { 0, 2, 6 };

		triangles[8] = { 2, 3, 6 };
		triangles[9] = { 3, 7, 6 };

		triangles[10] = { 4, 5, 0 };
		triangles[11] = { 5, 1, 0 };

		if (invertWindingOrder)
		{
			for (uint32 i = 0; i < arraysize(triangles); ++i)
			{
				uint32 tmp = triangles[i].b;
				triangles[i].b = triangles[i].c;
				triangles[i].c = tmp;
			}
		}

		uint32 baseVertex = (uint32)this->vertices.size();
		uint32 firstTriangle = (uint32)this->triangles.size();
		uint32 numTriangles = arraysize(triangles);

		this->vertices.resize(this->vertices.size() + arraysize(vertices));
		this->triangles.resize(this->triangles.size() + arraysize(triangles));

		for (uint32 i = 0; i < arraysize(vertices); ++i)
		{
			this->vertices[i + baseVertex].position = vertices[i].position;
		}

		memcpy(this->triangles.data() + firstTriangle, triangles, sizeof(triangles));

		return submesh_info{ firstTriangle, numTriangles, baseVertex };
	}
	else
	{
		vertex_3PUN vertices[24];
		indexed_triangle16 triangles[12];

		vertices[0] = { vec3(-radius, -radius, radius), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f) };
		vertices[1] = { vec3(radius, -radius, radius), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f) };
		vertices[2] = { vec3(-radius, radius, radius), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f) };
		vertices[3] = { vec3(radius, radius, radius), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f) };
		triangles[0] = { 0, 1, 2 };
		triangles[1] = { 1, 3, 2 };

		vertices[4] = { vec3(radius, -radius, radius), vec2(0.f, 0.f), vec3(1.f, 0.f, 0.f) };
		vertices[5] = { vec3(radius, -radius, -radius), vec2(1.f, 0.f), vec3(1.f, 0.f, 0.f) };
		vertices[6] = { vec3(radius, radius, radius), vec2(0.f, 1.f), vec3(1.f, 0.f, 0.f) };
		vertices[7] = { vec3(radius, radius, -radius), vec2(1.f, 1.f), vec3(1.f, 0.f, 0.f) };
		triangles[2] = { 4, 5, 6 };
		triangles[3] = { 5, 7, 6 };

		vertices[8] = { vec3(radius, -radius, -radius), vec2(0.f, 0.f), vec3(0.f, 0.f, -1.f) };
		vertices[9] = { vec3(-radius, -radius, -radius), vec2(1.f, 0.f), vec3(0.f, 0.f, -1.f) };
		vertices[10] = { vec3(radius, radius, -radius), vec2(0.f, 1.f), vec3(0.f, 0.f, -1.f) };
		vertices[11] = { vec3(-radius, radius, -radius), vec2(1.f, 1.f), vec3(0.f, 0.f, -1.f) };
		triangles[4] = { 8, 9, 10 };
		triangles[5] = { 9, 11, 10 };

		vertices[12] = { vec3(-radius, -radius, -radius), vec2(0.f, 0.f), vec3(-1.f, 0.f, 0.f) };
		vertices[13] = { vec3(-radius, -radius, radius), vec2(1.f, 0.f), vec3(-1.f, 0.f, 0.f) };
		vertices[14] = { vec3(-radius, radius, -radius), vec2(0.f, 1.f), vec3(-1.f, 0.f, 0.f) };
		vertices[15] = { vec3(-radius, radius, radius), vec2(1.f, 1.f), vec3(-1.f, 0.f, 0.f) };
		triangles[6] = { 12, 13, 14 };
		triangles[7] = { 13, 15, 14 };

		vertices[16] = { vec3(-radius, radius, radius), vec2(0.f, 0.f), vec3(0.f, 1.f, 0.f) };
		vertices[17] = { vec3(radius, radius, radius), vec2(1.f, 0.f), vec3(0.f, 1.f, 0.f) };
		vertices[18] = { vec3(-radius, radius, -radius), vec2(0.f, 1.f), vec3(0.f, 1.f, 0.f) };
		vertices[19] = { vec3(radius, radius, -radius), vec2(1.f, 1.f), vec3(0.f, 1.f, 0.f) };
		triangles[8] = { 16, 17, 18 };
		triangles[9] = { 17, 19, 18 };

		vertices[20] = { vec3(-radius, -radius, -radius), vec2(0.f, 0.f), vec3(0.f, -1.f, 0.f) };
		vertices[21] = { vec3(radius, -radius, -radius), vec2(1.f, 0.f), vec3(0.f, -1.f, 0.f) };
		vertices[22] = { vec3(-radius, -radius, radius), vec2(0.f, 1.f), vec3(0.f, -1.f, 0.f) };
		vertices[23] = { vec3(radius, -radius, radius), vec2(1.f, 1.f), vec3(0.f, -1.f, 0.f) };
		triangles[10] = { 20, 21, 22 };
		triangles[11] = { 21, 23, 22 };

		if (invertWindingOrder)
		{
			for (uint32 i = 0; i < arraysize(triangles); ++i)
			{
				uint32 tmp = triangles[i].b;
				triangles[i].b = triangles[i].c;
				triangles[i].c = tmp;
			}
		}

		uint32 baseVertex = (uint32)this->vertices.size();
		uint32 firstTriangle = (uint32)this->triangles.size();
		uint32 numTriangles = arraysize(triangles);

		this->vertices.resize(this->vertices.size() + arraysize(vertices));
		this->triangles.resize(this->triangles.size() + arraysize(triangles));

		for (uint32 i = 0; i < arraysize(vertices); ++i)
		{
			setPosition(this->vertices[i + baseVertex], vertices[i].position);
			setUV(this->vertices[i + baseVertex], vertices[i].uv);
			setNormal(this->vertices[i + baseVertex], vertices[i].normal);
		}

		memcpy(this->triangles.data() + firstTriangle, triangles, sizeof(triangles));

		return submesh_info{ firstTriangle, numTriangles, baseVertex };
	}
}

template<typename vertex_t>
inline submesh_info cpu_triangle_mesh<vertex_t>::pushSphere(uint16 slices, uint16 rows, float radius)
{
	assert(slices > 2);
	assert(rows > 0);

	float vertDeltaAngle = DirectX::XM_PI / (rows + 1);
	float horzDeltaAngle = 2.f * DirectX::XM_PI / slices;

	assert(slices * rows + 2 <= UINT16_MAX);
	vertex_3PUNT* vertices = new vertex_3PUNT[slices * rows + 2];
	indexed_triangle16* triangles = new indexed_triangle16[2 * rows * slices];

	uint16 vertIndex = 0;

	// Vertices.
	vertices[vertIndex].position = vec3(0.f, -radius, 0.f);
	vertices[vertIndex].normal = vec3(0.f, -1.f, 0.f);
	vertices[vertIndex].tangent = vec3(1.f, 0.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 0.f);
	++vertIndex;

	for (uint32 y = 0; y < rows; ++y)
	{
		float vertAngle = (y + 1) * vertDeltaAngle - DirectX::XM_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);
			vertices[vertIndex].position = pos;
			vertices[vertIndex].normal = nor;
			vertices[vertIndex].tangent = cross(vec3::up, vertices[vertIndex].normal).normalize();

			float u = asinf(vertexX) / DirectX::XM_PI + 0.5f;
			float v = asinf(vertexY) / DirectX::XM_PI + 0.5f;
			vertices[vertIndex].uv = vec2(u, v);
			++vertIndex;
		}
	}
	vertices[vertIndex].position = vec3(0.f, radius, 0.f);
	vertices[vertIndex].normal = vec3(0.f, 1.f, 0.f);
	vertices[vertIndex].tangent = vec3(1.f, 0.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 1.f);
	++vertIndex;

	assert(vertIndex == slices * rows + 2);

	uint32 triIndex = 0;

	// Indices.
	for (uint16 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle16{ 0, x + 1u, x + 2u };
	}
	triangles[triIndex++] = indexed_triangle16{ 0, slices, 1 };
	for (uint16 y = 0; y < rows - 1; ++y)
	{
		for (uint16 x = 0; x < slices - 1; ++x)
		{
			triangles[triIndex++] = indexed_triangle16{ y * slices + 1u + x, (y + 1u) * slices + 2u + x, y * slices + 2u + x };
			triangles[triIndex++] = indexed_triangle16{ y * slices + 1u + x, (y + 1u) * slices + 1u + x, (y + 1u) * slices + 2u + x };
		}
		triangles[triIndex++] = indexed_triangle16{ (uint16)(y * slices + slices), (uint16)((y + 1u) * slices + 1u), (uint16)(y * slices + 1u) };
		triangles[triIndex++] = indexed_triangle16{ (uint16)(y * slices + slices), (uint16)((y + 1u) * slices + slices), (uint16)((y + 1u) * slices + 1u) };
	}
	for (uint16 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle16{ vertIndex - 2u - x, vertIndex - 3u - x, vertIndex - 1u };
	}
	triangles[triIndex++] = indexed_triangle16{ vertIndex - 1u - slices, vertIndex - 2u, vertIndex - 1u };

	assert(triIndex == 2 * rows * slices);


	uint32 baseVertex = (uint32)this->vertices.size();
	uint32 firstTriangle = (uint32)this->triangles.size();
	uint32 numTriangles = triIndex;

	this->vertices.resize(this->vertices.size() + vertIndex);
	this->triangles.resize(this->triangles.size() + triIndex);

	for (uint32 i = 0; i < vertIndex; ++i)
	{
		setPosition(this->vertices[i + baseVertex], vertices[i].position);
		setUV(this->vertices[i + baseVertex], vertices[i].uv);
		setNormal(this->vertices[i + baseVertex], vertices[i].normal);
		setTangent(this->vertices[i + baseVertex], vertices[i].tangent);
	}

	memcpy(this->triangles.data() + firstTriangle, triangles, sizeof(indexed_triangle16) * triIndex);

	delete[] vertices;
	delete[] triangles;

	return submesh_info{ firstTriangle, numTriangles, baseVertex };
}

template<typename vertex_t>
inline submesh_info cpu_triangle_mesh<vertex_t>::pushCapsule(uint16 slices, uint16 rows, float height, float radius)
{
	assert(slices > 2);
	assert(rows > 0);
	assert(rows % 2 == 1);

	float vertDeltaAngle = DirectX::XM_PI / (rows + 1);
	float horzDeltaAngle = 2.f * DirectX::XM_PI / slices;
	float halfHeight = 0.5f * height;
	float texStretch = radius / (radius + halfHeight);

	assert(slices * (rows + 1) + 2 <= UINT16_MAX);
	vertex_3PUN* vertices = new vertex_3PUN[slices * (rows + 1) + 2];
	indexed_triangle16* triangles = new indexed_triangle16[2 * (rows + 1) * slices];

	uint32 vertIndex = 0;

	// Vertices.
	vertices[vertIndex].position = vec3(0.f, -radius - halfHeight, 0.f);
	vertices[vertIndex].normal = vec3(0.f, -1.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 0.f);
	++vertIndex;
	for (uint32 y = 0; y < rows / 2 + 1; ++y)
	{
		float vertAngle = (y + 1) * vertDeltaAngle - DirectX::XM_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius - halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);
			vertices[vertIndex].position = pos;
			vertices[vertIndex].normal = nor;

			float u = asinf(vertexX) / DirectX::XM_PI + 0.5f;
			float v = asinf(vertexY) / DirectX::XM_PI + 0.5f;
			vertices[vertIndex].uv = vec2(u, v * texStretch);
			++vertIndex;
		}
	}
	for (uint32 y = 0; y < rows / 2 + 1; ++y)
	{
		float vertAngle = (y + rows / 2 + 1) * vertDeltaAngle - DirectX::XM_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius + halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);
			vertices[vertIndex].position = pos;
			vertices[vertIndex].normal = nor;

			float u = asinf(vertexX) / DirectX::XM_PI + 0.5f;
			float v = asinf(vertexY) / DirectX::XM_PI + 0.5f;
			vertices[vertIndex].uv = vec2(u, v * texStretch + 1.f - texStretch);
			++vertIndex;
		}
	}
	vertices[vertIndex].position = vec3(0.f, radius + halfHeight, 0.f);
	vertices[vertIndex].normal = vec3(0.f, 1.f, 0.f);
	vertices[vertIndex].uv = vec2(0.5f, 1.f);
	++vertIndex;

	uint32 triIndex = 0;

	// Indices.
	for (uint32 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle16{ 0, x + 1, x + 2 };
	}
	triangles[triIndex++] = indexed_triangle16{ 0, slices, 1 };
	for (uint32 y = 0; y < rows; ++y)
	{
		for (uint32 x = 0; x < slices - 1; ++x)
		{
			triangles[triIndex++] = indexed_triangle16{ y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x };
			triangles[triIndex++] = indexed_triangle16{ y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x };
		}
		triangles[triIndex++] = indexed_triangle16{ y * slices + slices, (y + 1) * slices + 1, y * slices + 1 };
		triangles[triIndex++] = indexed_triangle16{ y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1 };
	}
	for (uint32 x = 0; x < slices - 1; ++x)
	{
		triangles[triIndex++] = indexed_triangle16{ vertIndex - 2 - x, vertIndex - 3 - x, vertIndex - 1 };
	}
	triangles[triIndex++] = indexed_triangle16{ vertIndex - 1 - slices, vertIndex - 2, vertIndex - 1 };


	uint32 baseVertex = (uint32)this->vertices.size();
	uint32 firstTriangle = (uint32)this->triangles.size();
	uint32 numTriangles = triIndex;

	this->vertices.resize(this->vertices.size() + vertIndex);
	this->triangles.resize(this->triangles.size() + triIndex);

	for (uint32 i = 0; i < vertIndex; ++i)
	{
		setPosition(this->vertices[i + baseVertex], vertices[i].position);
		setUV(this->vertices[i + baseVertex], vertices[i].uv);
		setNormal(this->vertices[i + baseVertex], vertices[i].normal);
	}

	memcpy(this->triangles.data() + firstTriangle, triangles, sizeof(indexed_triangle16) * triIndex);

	delete[] vertices;
	delete[] triangles;

	return submesh_info{ firstTriangle, numTriangles, baseVertex };
}

template<typename vertex_t>
static bounding_box computeBoundingBox(submesh_info submesh, const std::vector<vertex_t>& vertices, std::vector<indexed_triangle16>& triangles)
{
	bounding_box result = bounding_box::negativeInfinity();
	for (uint32 i = submesh.firstTriangle; i < submesh.firstTriangle + submesh.numTriangles; ++i)
	{
		result.grow(vertices[submesh.baseVertex + triangles[i].a].position);
		result.grow(vertices[submesh.baseVertex + triangles[i].b].position);
		result.grow(vertices[submesh.baseVertex + triangles[i].c].position);
	}

	return result;
}

template<typename vertex_t>
static void splitSubmeshHelper(submesh_info submesh, const std::vector<vertex_t>& vertices, std::vector<indexed_triangle16>& triangles, 
	float maxDim, uint32 maxNumTriangles, std::vector<submesh_info>& result)
{
	bool split = false;

	if (submesh.numTriangles >= 200)
	{
		bounding_box bb = computeBoundingBox(submesh, vertices, triangles);

		vec3 dim = bb.max - bb.min;

		float largest = dim.x;
		uint32 largestDim = 0;

		if (dim.y > largest)
		{
			largest = dim.y;
			largestDim = 1;
		}

		if (dim.z > largest)
		{
			largest = dim.z;
			largestDim = 2;
		}

		if (largest > maxDim || submesh.numTriangles > maxNumTriangles)
		{
			uint32 baseVertex = submesh.baseVertex;

			std::sort(triangles.begin() + submesh.firstTriangle, triangles.begin() + submesh.firstTriangle + submesh.numTriangles,
				[&vertices, largestDim, baseVertex](indexed_triangle16 a, indexed_triangle16 b)
				{
					float centerA = (vertices[baseVertex + a.a].position.data[largestDim]
								   + vertices[baseVertex + a.b].position.data[largestDim]
								   + vertices[baseVertex + a.c].position.data[largestDim]) / 3.f;
					float centerB = (vertices[baseVertex + b.a].position.data[largestDim]
								   + vertices[baseVertex + b.b].position.data[largestDim]
								   + vertices[baseVertex + b.c].position.data[largestDim]) / 3.f;
					return centerA < centerB;
				});

			submesh_info subA, subB;

			subA.firstTriangle = submesh.firstTriangle;
			subA.numTriangles = submesh.numTriangles / 2; // Mean split.
			subA.baseVertex = submesh.baseVertex;
			subA.materialIndex = submesh.materialIndex;

			subB.firstTriangle = submesh.firstTriangle + subA.numTriangles;
			subB.numTriangles = submesh.numTriangles - subA.numTriangles;
			subB.baseVertex = submesh.baseVertex;
			subB.materialIndex = submesh.materialIndex;

			splitSubmeshHelper(subA, vertices, triangles, maxDim, maxNumTriangles, result);
			splitSubmeshHelper(subB, vertices, triangles, maxDim, maxNumTriangles, result);

			split = true;
		}
	}

	if (!split)
	{
		result.push_back(submesh);
	}
}

template<typename vertex_t>
inline std::vector<submesh_info> cpu_triangle_mesh<vertex_t>::splitSubmesh(submesh_info submesh, float maxDim, uint32 maxNumTriangles)
{
	std::vector<submesh_info> result;
	result.reserve(1000);
	splitSubmeshHelper(submesh, vertices, triangles, maxDim, maxNumTriangles, result);
	return result;
}
