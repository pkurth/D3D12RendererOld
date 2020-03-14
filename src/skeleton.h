#pragma once

#include "math.h"

#define NO_PARENT ((uint32)-1)

struct skeleton_joint
{
	std::string name;
	uint32 parentID;
	trs bindTransform;	// Position of joint relative to model space.
	mat4 invBindMatrix; // Transforms from model space to joint space.
};

struct animation_skeleton
{
	std::vector<skeleton_joint> skeletonJoints;

	void getGlobalTransforms(const trs* localTransforms, trs* outGlobalTransforms, const trs& transform) const;
	void getSkinningMatrices(const trs* globalTransforms, mat4* outSkinningMatrices) const;
};

