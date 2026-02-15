#include "light3d/primitives.h"
#include <cmath>

namespace light3d {

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kTwoPi = 2.0f * kPi;

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------
MeshGeometry createBox(float width, float height, float depth) {
    MeshGeometry geom;

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;

    // 6 faces, each with 4 unique vertices (separate normals per face)
    struct FaceDef {
        Vec3 corners[4];
        Vec3 normal;
    };
    // clang-format off
    FaceDef faces[6];
    // +X
    faces[0] = {{Vec3( hw,-hh,-hd), Vec3( hw, hh,-hd), Vec3( hw, hh, hd), Vec3( hw,-hh, hd)}, Vec3( 1, 0, 0)};
    // -X
    faces[1] = {{Vec3(-hw,-hh, hd), Vec3(-hw, hh, hd), Vec3(-hw, hh,-hd), Vec3(-hw,-hh,-hd)}, Vec3(-1, 0, 0)};
    // +Y
    faces[2] = {{Vec3(-hw, hh,-hd), Vec3(-hw, hh, hd), Vec3( hw, hh, hd), Vec3( hw, hh,-hd)}, Vec3( 0, 1, 0)};
    // -Y
    faces[3] = {{Vec3(-hw,-hh, hd), Vec3(-hw,-hh,-hd), Vec3( hw,-hh,-hd), Vec3( hw,-hh, hd)}, Vec3( 0,-1, 0)};
    // +Z
    faces[4] = {{Vec3(-hw,-hh, hd), Vec3( hw,-hh, hd), Vec3( hw, hh, hd), Vec3(-hw, hh, hd)}, Vec3( 0, 0, 1)};
    // -Z
    faces[5] = {{Vec3( hw,-hh,-hd), Vec3(-hw,-hh,-hd), Vec3(-hw, hh,-hd), Vec3( hw, hh,-hd)}, Vec3( 0, 0,-1)};
    // clang-format on

    float uvCorners[4][2] = {{0,0},{1,0},{1,1},{0,1}};

    for (int fi = 0; fi < 6; ++fi) {
        int base = static_cast<int>(geom.points.size());
        for (int vi = 0; vi < 4; ++vi) {
            geom.points.push_back(faces[fi].corners[vi]);
            geom.normals.push_back(faces[fi].normal);
            geom.uvs.push_back(Vec3(uvCorners[vi][0], uvCorners[vi][1], 0.0f));
        }
        geom.faceVertexCounts.push_back(4);
        geom.faceVertexIndices.push_back(base);
        geom.faceVertexIndices.push_back(base + 1);
        geom.faceVertexIndices.push_back(base + 2);
        geom.faceVertexIndices.push_back(base + 3);
        geom.faceMaterialIds.push_back(0);
    }

    return geom;
}

// ---------------------------------------------------------------------------
// Sphere (UV sphere)
// ---------------------------------------------------------------------------
MeshGeometry createSphere(float radius, int stacks, int slices) {
    MeshGeometry geom;

    // Generate vertices: (stacks+1) rings of (slices+1) points each
    for (int i = 0; i <= stacks; ++i) {
        float phi = kPi * static_cast<float>(i) / static_cast<float>(stacks);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);
        float v = static_cast<float>(i) / static_cast<float>(stacks);

        for (int j = 0; j <= slices; ++j) {
            float theta = kTwoPi * static_cast<float>(j) / static_cast<float>(slices);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);
            float u = static_cast<float>(j) / static_cast<float>(slices);

            Vec3 n(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            geom.points.push_back(Vec3(n.x * radius, n.y * radius, n.z * radius));
            geom.normals.push_back(n);
            geom.uvs.push_back(Vec3(u, v, 0.0f));
        }
    }

    // Generate quad faces
    int cols = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int a = i * cols + j;
            int b = a + 1;
            int c = a + cols + 1;
            int d = a + cols;

            geom.faceVertexCounts.push_back(4);
            geom.faceVertexIndices.push_back(a);
            geom.faceVertexIndices.push_back(b);
            geom.faceVertexIndices.push_back(c);
            geom.faceVertexIndices.push_back(d);
            geom.faceMaterialIds.push_back(0);
        }
    }

    return geom;
}

// ---------------------------------------------------------------------------
// Cylinder
// ---------------------------------------------------------------------------
MeshGeometry createCylinder(float radius, float height, int slices) {
    MeshGeometry geom;

    float hh = height * 0.5f;

    // --- Side vertices: 2 rings ---
    int sideBase = static_cast<int>(geom.points.size());
    for (int ring = 0; ring < 2; ++ring) {
        float y = (ring == 0) ? -hh : hh;
        float v = static_cast<float>(ring);
        for (int j = 0; j <= slices; ++j) {
            float theta = kTwoPi * static_cast<float>(j) / static_cast<float>(slices);
            float ct = std::cos(theta);
            float st = std::sin(theta);
            float u = static_cast<float>(j) / static_cast<float>(slices);

            geom.points.push_back(Vec3(ct * radius, y, st * radius));
            geom.normals.push_back(Vec3(ct, 0.0f, st));
            geom.uvs.push_back(Vec3(u, v, 0.0f));
        }
    }

    // Side quads
    int cols = slices + 1;
    for (int j = 0; j < slices; ++j) {
        int a = sideBase + j;
        int b = a + 1;
        int c = a + cols + 1;
        int d = a + cols;
        geom.faceVertexCounts.push_back(4);
        geom.faceVertexIndices.push_back(a);
        geom.faceVertexIndices.push_back(b);
        geom.faceVertexIndices.push_back(c);
        geom.faceVertexIndices.push_back(d);
        geom.faceMaterialIds.push_back(0);
    }

    // --- Cap vertices & faces ---
    for (int cap = 0; cap < 2; ++cap) {
        float y = (cap == 0) ? -hh : hh;
        Vec3 n(0.0f, (cap == 0) ? -1.0f : 1.0f, 0.0f);

        // Center vertex
        int centerIdx = static_cast<int>(geom.points.size());
        geom.points.push_back(Vec3(0.0f, y, 0.0f));
        geom.normals.push_back(n);
        geom.uvs.push_back(Vec3(0.5f, 0.5f, 0.0f));

        // Ring vertices
        int ringBase = static_cast<int>(geom.points.size());
        for (int j = 0; j <= slices; ++j) {
            float theta = kTwoPi * static_cast<float>(j) / static_cast<float>(slices);
            float ct = std::cos(theta);
            float st = std::sin(theta);
            geom.points.push_back(Vec3(ct * radius, y, st * radius));
            geom.normals.push_back(n);
            geom.uvs.push_back(Vec3(ct * 0.5f + 0.5f, st * 0.5f + 0.5f, 0.0f));
        }

        // Triangle fan
        for (int j = 0; j < slices; ++j) {
            geom.faceVertexCounts.push_back(3);
            if (cap == 0) {
                // Bottom cap: wind CW from below
                geom.faceVertexIndices.push_back(centerIdx);
                geom.faceVertexIndices.push_back(ringBase + j + 1);
                geom.faceVertexIndices.push_back(ringBase + j);
            } else {
                // Top cap: wind CCW from above
                geom.faceVertexIndices.push_back(centerIdx);
                geom.faceVertexIndices.push_back(ringBase + j);
                geom.faceVertexIndices.push_back(ringBase + j + 1);
            }
            geom.faceMaterialIds.push_back(0);
        }
    }

    return geom;
}

// ---------------------------------------------------------------------------
// Plane
// ---------------------------------------------------------------------------
MeshGeometry createPlane(float width, float depth,
                         int subdivisionsX, int subdivisionsZ) {
    MeshGeometry geom;

    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    int nx = subdivisionsX + 1;
    int nz = subdivisionsZ + 1;

    for (int iz = 0; iz < nz; ++iz) {
        float tz = static_cast<float>(iz) / static_cast<float>(subdivisionsZ);
        float z = -hd + tz * depth;
        for (int ix = 0; ix < nx; ++ix) {
            float tx = static_cast<float>(ix) / static_cast<float>(subdivisionsX);
            float x = -hw + tx * width;

            geom.points.push_back(Vec3(x, 0.0f, z));
            geom.normals.push_back(Vec3(0.0f, 1.0f, 0.0f));
            geom.uvs.push_back(Vec3(tx, tz, 0.0f));
        }
    }

    for (int iz = 0; iz < subdivisionsZ; ++iz) {
        for (int ix = 0; ix < subdivisionsX; ++ix) {
            int a = iz * nx + ix;
            int b = a + 1;
            int c = a + nx + 1;
            int d = a + nx;

            geom.faceVertexCounts.push_back(4);
            geom.faceVertexIndices.push_back(a);
            geom.faceVertexIndices.push_back(b);
            geom.faceVertexIndices.push_back(c);
            geom.faceVertexIndices.push_back(d);
            geom.faceMaterialIds.push_back(0);
        }
    }

    return geom;
}

// ---------------------------------------------------------------------------
// Torus
// ---------------------------------------------------------------------------
MeshGeometry createTorus(float majorRadius, float minorRadius,
                         int majorSegments, int minorSegments) {
    MeshGeometry geom;

    int majCols = majorSegments + 1;
    int minCols = minorSegments + 1;

    for (int i = 0; i <= majorSegments; ++i) {
        float u = static_cast<float>(i) / static_cast<float>(majorSegments);
        float theta = u * kTwoPi;
        float ct = std::cos(theta);
        float st = std::sin(theta);

        for (int j = 0; j <= minorSegments; ++j) {
            float v = static_cast<float>(j) / static_cast<float>(minorSegments);
            float phi = v * kTwoPi;
            float cp = std::cos(phi);
            float sp = std::sin(phi);

            float r = majorRadius + minorRadius * cp;
            Vec3 pos(r * ct, minorRadius * sp, r * st);
            Vec3 n(cp * ct, sp, cp * st);

            geom.points.push_back(pos);
            geom.normals.push_back(n);
            geom.uvs.push_back(Vec3(u, v, 0.0f));
        }
    }

    for (int i = 0; i < majorSegments; ++i) {
        for (int j = 0; j < minorSegments; ++j) {
            int a = i * minCols + j;
            int b = a + 1;
            int c = a + minCols + 1;
            int d = a + minCols;

            geom.faceVertexCounts.push_back(4);
            geom.faceVertexIndices.push_back(a);
            geom.faceVertexIndices.push_back(b);
            geom.faceVertexIndices.push_back(c);
            geom.faceVertexIndices.push_back(d);
            geom.faceMaterialIds.push_back(0);
        }
    }

    return geom;
}

} // namespace light3d
