#include "light3d/skeleton.h"

namespace light3d {

SkeletonPrim::SkeletonPrim(const SdfPath& path, const std::string& name)
    : Xform(path, name) {}

int SkeletonPrim::addJoint(const std::string& jointName, int parentIndex,
                           const Mat4& inverseBindMatrix, const Mat4& restLocalMatrix) {
    int index = static_cast<int>(joints_.size());
    Joint j;
    j.name = jointName;
    j.parentIndex = parentIndex;
    j.inverseBindMatrix = inverseBindMatrix;
    j.restLocalMatrix = restLocalMatrix;
    joints_.push_back(j);
    jointNameMap_[jointName] = index;

    // Resize working arrays
    jointLocalTransforms_.push_back(restLocalMatrix);
    jointWorldTransforms_.push_back(Mat4::identity());
    skinningMatrices_.push_back(Mat4::identity());

    return index;
}

int SkeletonPrim::getJointIndex(const std::string& jointName) const {
    auto it = jointNameMap_.find(jointName);
    return (it != jointNameMap_.end()) ? it->second : -1;
}

void SkeletonPrim::setJointLocalTransform(int jointIndex, const Mat4& localMatrix) {
    jointLocalTransforms_[static_cast<size_t>(jointIndex)] = localMatrix;
}

void SkeletonPrim::computeSkinningMatrices() {
    size_t n = joints_.size();
    for (size_t i = 0; i < n; ++i) {
        int parent = joints_[i].parentIndex;
        if (parent < 0) {
            jointWorldTransforms_[i] = jointLocalTransforms_[i];
        } else {
            jointWorldTransforms_[i] = jointWorldTransforms_[static_cast<size_t>(parent)] * jointLocalTransforms_[i];
        }
        skinningMatrices_[i] = jointWorldTransforms_[i] * joints_[i].inverseBindMatrix;
    }
}

} // namespace light3d
