#include "test_helpers.h"
#include <light3d/primitives.h>
#include <light3d/math.h>

using namespace light3d;

// Helper: check that a MeshGeometry is internally consistent.
static bool isConsistent(const MeshGeometry& g) {
    // Sum of faceVertexCounts == faceVertexIndices.size()
    int total = 0;
    for (int c : g.faceVertexCounts) total += c;
    if (total != static_cast<int>(g.faceVertexIndices.size())) return false;

    // faceMaterialIds.size() == faceVertexCounts.size()
    if (g.faceMaterialIds.size() != g.faceVertexCounts.size()) return false;

    // normals and uvs should match points count
    if (g.normals.size() != g.points.size()) return false;
    if (g.uvs.size() != g.points.size()) return false;

    // All indices in range
    int nv = static_cast<int>(g.points.size());
    for (int idx : g.faceVertexIndices)
        if (idx < 0 || idx >= nv) return false;

    return true;
}

// --- Box ---

TEST(box_default) {
    MeshGeometry g = createBox();
    ASSERT(isConsistent(g));
    ASSERT_EQ(g.faceCount(), 6u);
    ASSERT_EQ(g.vertexCount(), 24u); // 6 faces * 4 verts
}

TEST(box_custom_size) {
    MeshGeometry g = createBox(2.0f, 3.0f, 4.0f);
    ASSERT(isConsistent(g));
    // Check that extent covers expected range
    float minX = 1e30f, maxX = -1e30f;
    for (auto& p : g.points) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    ASSERT_NEAR(maxX - minX, 2.0f, 1e-5f);
}

TEST(box_normals_unit_length) {
    MeshGeometry g = createBox();
    for (auto& n : g.normals) {
        float len = length(n);
        ASSERT_NEAR(len, 1.0f, 1e-4f);
    }
}

// --- Sphere ---

TEST(sphere_default) {
    MeshGeometry g = createSphere();
    ASSERT(isConsistent(g));
    ASSERT(g.faceCount() > 0);
    ASSERT(g.vertexCount() > 0);
}

TEST(sphere_all_points_on_surface) {
    float radius = 2.0f;
    MeshGeometry g = createSphere(radius, 8, 16);
    for (auto& p : g.points) {
        float dist = length(p);
        ASSERT_NEAR(dist, radius, 1e-4f);
    }
}

TEST(sphere_normals_unit_length) {
    MeshGeometry g = createSphere(1.0f, 8, 16);
    for (auto& n : g.normals) {
        float len = length(n);
        ASSERT_NEAR(len, 1.0f, 1e-4f);
    }
}

// --- Cylinder ---

TEST(cylinder_default) {
    MeshGeometry g = createCylinder();
    ASSERT(isConsistent(g));
    ASSERT(g.faceCount() > 0);
}

TEST(cylinder_height) {
    float height = 4.0f;
    MeshGeometry g = createCylinder(0.5f, height, 16);
    ASSERT(isConsistent(g));
    float minY = 1e30f, maxY = -1e30f;
    for (auto& p : g.points) {
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }
    ASSERT_NEAR(maxY - minY, height, 1e-4f);
}

// --- Plane ---

TEST(plane_default) {
    MeshGeometry g = createPlane();
    ASSERT(isConsistent(g));
    ASSERT_EQ(g.faceCount(), 1u);   // 1x1 subdivisions = 1 quad
    ASSERT_EQ(g.vertexCount(), 4u);
}

TEST(plane_subdivided) {
    MeshGeometry g = createPlane(1.0f, 1.0f, 3, 3);
    ASSERT(isConsistent(g));
    ASSERT_EQ(g.faceCount(), 9u);   // 3x3 quads
    ASSERT_EQ(g.vertexCount(), 16u); // 4x4 grid
}

TEST(plane_all_normals_up) {
    MeshGeometry g = createPlane(2.0f, 2.0f, 2, 2);
    for (auto& n : g.normals) {
        ASSERT_NEAR(n.x, 0.0f, 1e-5f);
        ASSERT_NEAR(n.y, 1.0f, 1e-5f);
        ASSERT_NEAR(n.z, 0.0f, 1e-5f);
    }
}

// --- Torus ---

TEST(torus_default) {
    MeshGeometry g = createTorus();
    ASSERT(isConsistent(g));
    ASSERT(g.faceCount() > 0);
}

TEST(torus_normals_unit_length) {
    MeshGeometry g = createTorus(1.0f, 0.3f, 16, 8);
    for (auto& n : g.normals) {
        float len = length(n);
        ASSERT_NEAR(len, 1.0f, 1e-4f);
    }
}

int main() {
    std::printf("=== Primitives Tests ===\n");
    RUN_TESTS();
}
