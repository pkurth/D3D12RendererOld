#pragma once

#include "common.h"
#include "math.h"

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>


struct vertex_3PUN
{
	vec3 position;
	vec2 uv;
	vec3 normal;
};

struct indexed_triangle32
{
	uint32 a, b, c;
};


template <typename vertex_t>
struct triangle_mesh
{
	std::vector<vertex_t> vertices;
	std::vector<indexed_triangle32> triangles;

	void loadFromFile(const std::string& filename);

private:
	struct general_{};
	struct special_ : general_ {};
	template <typename> struct int_ { typedef int type; };

	template <typename int_<decltype(vertex_t::position)>::type = 0>
	void setPosition(vertex_t& v, vec3 p, special_)
	{
		v.position = p;
	}

	void setPosition(vertex_t& v, vec3 p, general_)
	{
	}

	template <typename int_<decltype(vertex_t::normal)>::type = 0>
	void setNormal(vertex_t& v, vec3 n, special_)
	{
		v.normal = n;
	}

	void setNormal(vertex_t& v, vec3 n, general_)
	{
	}

	template <typename int_<decltype(vertex_t::uv)>::type = 0>
	void setUV(vertex_t& v, vec2 uv, special_)
	{
		v.uv = uv;
	}

	void setUV(vertex_t& v, vec2 uv, general_)
	{
	}
};

template<typename vertex_t>
inline void triangle_mesh<vertex_t>::loadFromFile(const std::string& filename)
{
	std::filesystem::path path(filename);
	assert(std::filesystem::exists(path));

	std::filesystem::path exportPath = path;
	exportPath.replace_extension("assbin");

	Assimp::Importer importer;
	const aiScene* scene;

	// Check if a preprocessed file exists.
	if (std::filesystem::exists(exportPath) && std::filesystem::is_regular_file(exportPath))
	{
		scene = importer.ReadFile(exportPath.string(), 0);
	}
	else
	{
		// File has not been preprocessed yet. Import and processes the file.
		importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

		uint32 preprocessFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_ConvertToLeftHanded;
		scene = importer.ReadFile(path.string(), preprocessFlags);

		if (scene)
		{
			// Export the preprocessed scene file for faster loading next time.
			Assimp::Exporter exporter;
			exporter.Export(scene, "assbin", exportPath.string(), preprocessFlags);
		}
	}

	assert(scene);
	assert(scene->mNumMeshes > 0);

	const aiMesh* mesh = scene->mMeshes[0];

	vertices.resize(mesh->mNumVertices);
	triangles.resize(mesh->mNumFaces);

	vec3 position(0.f, 0.f, 0.f);
	vec3 normal(0.f, 0.f, 0.f);
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
		if (mesh->HasTextureCoords(0))
		{
			uv = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}

		setPosition(vertices[i], position, special_());
		setNormal(vertices[i], normal, special_());
		setUV(vertices[i], uv, special_());
	}


	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		triangles[i].a = face.mIndices[0];
		triangles[i].b = face.mIndices[1];
		triangles[i].c = face.mIndices[2];
	}
}
