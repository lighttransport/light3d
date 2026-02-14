#include "light3d/stage.h"
#include "light3d/xform.h"
#include "light3d/mesh_data.h"
#include "light3d/skeleton.h"
#include "light3d/skel_animation.h"

namespace light3d {

// --- Path utilities ---

std::string Stage::parentPath(const SdfPath& path) {
    auto pos = path.rfind('/');
    if (pos == 0 || pos == std::string::npos) return "";
    return path.substr(0, pos);
}

std::string Stage::primName(const SdfPath& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

// --- Prim creation helpers ---

Prim* Stage::ensureParents(const SdfPath& path) {
    std::string parent = parentPath(path);
    if (parent.empty()) return nullptr;

    // Already exists?
    auto it = pathMap_.find(parent);
    if (it != pathMap_.end()) return it->second;

    // Recursively ensure grandparents, then create intermediate Xform
    Prim* grandparent = ensureParents(parent);

    auto xform = std::make_unique<Xform>(parent, primName(parent));
    Xform* raw = xform.get();
    pathMap_[parent] = raw;
    prims_.push_back(std::move(xform));

    if (grandparent) {
        raw->setParent(grandparent);
        grandparent->addChild(raw);
    } else {
        rootPrims_.push_back(raw);
    }

    return raw;
}

template <typename T>
T* Stage::definePrim(const SdfPath& path) {
    // Check if already exists
    auto it = pathMap_.find(path);
    if (it != pathMap_.end()) {
        return dynamic_cast<T*>(it->second);
    }

    Prim* parent = ensureParents(path);

    auto prim = std::make_unique<T>(path, primName(path));
    T* raw = prim.get();
    pathMap_[path] = raw;
    prims_.push_back(std::move(prim));

    if (parent) {
        raw->setParent(parent);
        parent->addChild(raw);
    } else {
        rootPrims_.push_back(raw);
    }

    return raw;
}

Prim* Stage::getPrimAtPath(const SdfPath& path) const {
    auto it = pathMap_.find(path);
    return (it != pathMap_.end()) ? it->second : nullptr;
}

// --- Traversal ---

void Stage::traverse(const std::function<void(Prim&)>& visitor) const {
    for (Prim* root : rootPrims_) {
        traverseRecursive(*root, visitor);
    }
}

void Stage::traverseRecursive(Prim& prim, const std::function<void(Prim&)>& visitor) const {
    visitor(prim);
    for (Prim* child : prim.getChildren()) {
        traverseRecursive(*child, visitor);
    }
}

// --- World transforms ---

void Stage::updateWorldTransforms() {
    Mat4 identity = Mat4::identity();
    for (Prim* root : rootPrims_) {
        updateWorldTransformsRecursive(*root, identity);
    }
}

void Stage::updateWorldTransformsRecursive(Prim& prim, const Mat4& parentWorld) {
    Mat4 world = parentWorld;

    auto* xform = dynamic_cast<Xform*>(&prim);
    if (xform) {
        world = parentWorld * xform->getLocalMatrix();
        xform->setWorldMatrix(world);
    }

    for (Prim* child : prim.getChildren()) {
        updateWorldTransformsRecursive(*child, world);
    }
}

// --- Explicit template instantiations ---

template Xform* Stage::definePrim<Xform>(const SdfPath&);
template MeshPrim* Stage::definePrim<MeshPrim>(const SdfPath&);
template SkeletonPrim* Stage::definePrim<SkeletonPrim>(const SdfPath&);
template SkelAnimationPrim* Stage::definePrim<SkelAnimationPrim>(const SdfPath&);

} // namespace light3d
