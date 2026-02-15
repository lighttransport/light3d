#pragma once

#include "mesh_data.h"
#include "material.h"
#include "texture.h"
#include <string>

namespace light3d {

struct ObjLoadResult {
    MeshGeometry geometry;
    MaterialLibrary materials;
    TextureLibrary textures;
    bool success = false;
    std::string errorMessage;
};

struct ObjLoadOptions {
    bool loadTextures = true;
    bool flipUVsVertically = false;
    bool verbose = false;
};

// Load an OBJ file. basePath is the directory for resolving MTL/texture paths
// (defaults to the directory containing objFilepath).
ObjLoadResult loadObj(const std::string& objFilepath,
                      const std::string& basePath = "");

ObjLoadResult loadObjWithOptions(const std::string& objFilepath,
                                 const ObjLoadOptions& options,
                                 const std::string& basePath = "");

} // namespace light3d
