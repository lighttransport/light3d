#pragma once

#include "xform.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace light3d {

struct Joint {
    std::string name;
    int parentIndex = -1;          // -1 = root joint
    Mat4 inverseBindMatrix = Mat4::identity();
    Mat4 restLocalMatrix = Mat4::identity();
};

class SkeletonPrim : public Xform {
public:
    SkeletonPrim(const SdfPath& path, const std::string& name);
    ~SkeletonPrim() override = default;

    PrimType getType() const override { return PrimType::Skeleton; }

    // Joint management
    int addJoint(const std::string& jointName, int parentIndex,
                 const Mat4& inverseBindMatrix, const Mat4& restLocalMatrix);
    int getJointIndex(const std::string& jointName) const;
    size_t jointCount() const { return joints_.size(); }
    const Joint& getJoint(int index) const { return joints_[static_cast<size_t>(index)]; }

    const std::vector<Joint>& getJoints() const { return joints_; }

    // Set current local transforms for joints (e.g., from animation)
    void setJointLocalTransform(int jointIndex, const Mat4& localMatrix);

    // Compute final skinning matrices: jointWorldMatrix * inverseBindMatrix
    // Call this after setting all joint local transforms
    void computeSkinningMatrices();

    const std::vector<Mat4>& getSkinningMatrices() const { return skinningMatrices_; }

    // Optional animation source path
    void setAnimationSourcePath(const SdfPath& path) { animSourcePath_ = path; }
    const SdfPath& getAnimationSourcePath() const { return animSourcePath_; }

private:
    std::vector<Joint> joints_;
    std::unordered_map<std::string, int> jointNameMap_;
    std::vector<Mat4> jointLocalTransforms_;
    std::vector<Mat4> jointWorldTransforms_;
    std::vector<Mat4> skinningMatrices_;
    SdfPath animSourcePath_;
};

} // namespace light3d
