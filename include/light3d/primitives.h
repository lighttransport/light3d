#pragma once

#include "mesh_data.h"

namespace light3d {

// Box centered at origin with given dimensions.
MeshGeometry createBox(float width = 1.0f, float height = 1.0f, float depth = 1.0f);

// UV sphere centered at origin.
MeshGeometry createSphere(float radius = 0.5f, int stacks = 16, int slices = 32);

// Cylinder along Y axis, centered at origin.
MeshGeometry createCylinder(float radius = 0.5f, float height = 1.0f, int slices = 32);

// Plane in the XZ plane, centered at origin, facing +Y.
MeshGeometry createPlane(float width = 1.0f, float depth = 1.0f,
                         int subdivisionsX = 1, int subdivisionsZ = 1);

// Torus centered at origin, lying in the XZ plane.
MeshGeometry createTorus(float majorRadius = 0.5f, float minorRadius = 0.15f,
                         int majorSegments = 32, int minorSegments = 16);

} // namespace light3d
