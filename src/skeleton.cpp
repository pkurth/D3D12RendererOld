#include "pch.h"
#include "skeleton.h"

void animation_skeleton::getGlobalTransforms(const trs* localTransforms, trs* outGlobalTransforms, const trs& transform) const
{
	uint32 numJoints = (uint32)skeletonJoints.size();

	for (uint32 jointID = 0; jointID < numJoints; ++jointID)
	{
		const skeleton_joint& skelJoint = skeletonJoints[jointID];
		if (skelJoint.parentID != NO_PARENT)
		{
			assert(jointID > skelJoint.parentID); // Parent already processed.
			outGlobalTransforms[jointID] = outGlobalTransforms[skelJoint.parentID] * localTransforms[jointID];
		}
		else
		{
			outGlobalTransforms[jointID] = transform * localTransforms[jointID];
		}
	}
}

void animation_skeleton::getSkinningMatrices(const trs* globalTransforms, mat4* outSkinningMatrices) const
{
	uint32 numJoints = (uint32)skeletonJoints.size();

	for (uint32 jointID = 0; jointID < numJoints; ++jointID)
	{
		const trs& t = globalTransforms[jointID];
		outSkinningMatrices[jointID] = createModelMatrix(t.position, t.rotation, t.scale) * skeletonJoints[jointID].invBindMatrix;
	}
}
