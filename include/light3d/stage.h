#pragma once

#include "prim.h"
#include "math.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace light3d {

class Xform;

class Stage {
public:
    Stage() = default;
    ~Stage() = default;

    // Create a prim of type T at the given path, auto-creating intermediate Xform parents
    template <typename T>
    T* definePrim(const SdfPath& path);

    Prim* getPrimAtPath(const SdfPath& path) const;

    const std::vector<Prim*>& getRootPrims() const { return rootPrims_; }

    // Traverse all prims depth-first, calling visitor on each
    void traverse(const std::function<void(Prim&)>& visitor) const;

    // Recompute world transforms for all Xform-derived prims
    void updateWorldTransforms();

    size_t primCount() const { return prims_.size(); }

private:
    // Ensure intermediate prims exist along the path, return parent for the leaf
    Prim* ensureParents(const SdfPath& path);
    static std::string parentPath(const SdfPath& path);
    static std::string primName(const SdfPath& path);

    void traverseRecursive(Prim& prim, const std::function<void(Prim&)>& visitor) const;
    void updateWorldTransformsRecursive(Prim& prim, const Mat4& parentWorld);

    std::vector<std::unique_ptr<Prim>> prims_;
    std::unordered_map<std::string, Prim*> pathMap_;
    std::vector<Prim*> rootPrims_;
};

} // namespace light3d
