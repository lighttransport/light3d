#pragma once

#include "xform.h"
#include <vector>
#include <cstdint>

namespace light3d {

struct MeshGeometry {
    std::vector<Vec3> points;
    std::vector<Vec3> normals;
    std::vector<Vec3> uvs;           // stored as Vec3 for flexibility (u, v, 0)
    std::vector<int> faceVertexCounts;   // e.g. {3, 3, 4} = tri, tri, quad
    std::vector<int> faceVertexIndices;  // indices into points

    // Skinning data (per-vertex)
    std::vector<Vec4> jointIndices;  // up to 4 joint influences per vertex (as float)
    std::vector<Vec4> jointWeights;  // corresponding weights

    size_t vertexCount() const { return points.size(); }
    size_t faceCount() const { return faceVertexCounts.size(); }
    bool hasSkinning() const { return !jointWeights.empty(); }
};

class MeshPrim : public Xform {
public:
    MeshPrim(const SdfPath& path, const std::string& name);
    ~MeshPrim() override = default;

    PrimType getType() const override { return PrimType::Mesh; }

    MeshGeometry& getGeometry() { return geometry_; }
    const MeshGeometry& getGeometry() const { return geometry_; }

    // Optional reference to skeleton path for skinning
    void setSkeletonPath(const SdfPath& path) { skeletonPath_ = path; }
    const SdfPath& getSkeletonPath() const { return skeletonPath_; }

private:
    MeshGeometry geometry_;
    SdfPath skeletonPath_;
};

} // namespace light3d
