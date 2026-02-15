#include "light3d/obj_loader.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace light3d {

// --- Helper utilities ---

static std::string getDirectory(const std::string& filepath) {
    auto pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return filepath.substr(0, pos);
}

static std::string trimWhitespace(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Vertex key for deduplication: (posIdx, uvIdx, normIdx)
struct VertexKey {
    int pos, uv, norm;
    bool operator==(const VertexKey& o) const {
        return pos == o.pos && uv == o.uv && norm == o.norm;
    }
};

struct VertexKeyHash {
    size_t operator()(const VertexKey& k) const {
        size_t h = std::hash<int>()(k.pos);
        h ^= std::hash<int>()(k.uv) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.norm) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// Parse a face vertex token like "v", "v/vt", "v/vt/vn", "v//vn"
// Returns 1-based indices (0 = not specified).
static void parseFaceVertex(const std::string& token, int& posIdx, int& uvIdx, int& normIdx) {
    posIdx = 0; uvIdx = 0; normIdx = 0;

    // Find slashes
    auto s1 = token.find('/');
    if (s1 == std::string::npos) {
        // Just "v"
        posIdx = std::stoi(token);
        return;
    }

    posIdx = std::stoi(token.substr(0, s1));

    auto s2 = token.find('/', s1 + 1);
    if (s2 == std::string::npos) {
        // "v/vt"
        uvIdx = std::stoi(token.substr(s1 + 1));
        return;
    }

    // "v/vt/vn" or "v//vn"
    std::string uvStr = token.substr(s1 + 1, s2 - s1 - 1);
    if (!uvStr.empty()) {
        uvIdx = std::stoi(uvStr);
    }
    normIdx = std::stoi(token.substr(s2 + 1));
}

// Resolve negative OBJ index (1-based, negative = relative to end)
static int resolveIndex(int idx, int count) {
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return 0; // should not happen for valid OBJ
}

// --- MTL parser ---

struct MtlParseResult {
    MaterialLibrary materials;
    TextureLibrary textures;
    // Maps material name -> texture file paths (resolved)
    std::unordered_map<std::string, std::string> baseColorTexPaths;
    std::unordered_map<std::string, std::string> normalTexPaths;
    std::unordered_map<std::string, std::string> emissiveTexPaths;
};

static MtlParseResult parseMtlFile(const std::string& mtlPath,
                                   const std::string& basePath,
                                   const ObjLoadOptions& options) {
    MtlParseResult result;

    std::ifstream file(mtlPath);
    if (!file.is_open()) return result;

    Material* current = nullptr;
    std::string currentName;
    std::string line;

    while (std::getline(file, line)) {
        line = trimWhitespace(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "newmtl") {
            std::string name;
            std::getline(iss >> std::ws, name);
            name = trimWhitespace(name);
            currentName = name;

            Material mat;
            mat.name = name;
            int id = result.materials.addMaterial(mat);
            current = result.materials.getMaterial(id);
        } else if (!current) {
            continue;
        } else if (keyword == "Kd") {
            float r, g, b;
            iss >> r >> g >> b;
            current->baseColor = Vec3(r, g, b);
        } else if (keyword == "Ks") {
            float r, g, b;
            iss >> r >> g >> b;
            // Use magnitude of Ks as metallic approximation
            float mag = std::sqrt(r * r + g * g + b * b) / std::sqrt(3.0f);
            current->metallic = std::min(mag, 1.0f);
        } else if (keyword == "Ke") {
            float r, g, b;
            iss >> r >> g >> b;
            current->emissive = Vec3(r, g, b);
        } else if (keyword == "Ns") {
            float ns;
            iss >> ns;
            // Ns typically 0-1000. Convert: roughness = 1 - sqrt(Ns/1000)
            current->roughness = 1.0f - std::sqrt(std::min(ns, 1000.0f) / 1000.0f);
        } else if (keyword == "d") {
            float d;
            iss >> d;
            current->alpha = d;
        } else if (keyword == "Tr") {
            float tr;
            iss >> tr;
            current->alpha = 1.0f - tr;
        } else if (keyword == "map_Kd") {
            std::string texFile;
            std::getline(iss >> std::ws, texFile);
            texFile = trimWhitespace(texFile);
            result.baseColorTexPaths[currentName] = basePath + "/" + texFile;
        } else if (keyword == "map_Bump" || keyword == "bump") {
            std::string texFile;
            std::getline(iss >> std::ws, texFile);
            texFile = trimWhitespace(texFile);
            result.normalTexPaths[currentName] = basePath + "/" + texFile;
        } else if (keyword == "map_Ke") {
            std::string texFile;
            std::getline(iss >> std::ws, texFile);
            texFile = trimWhitespace(texFile);
            result.emissiveTexPaths[currentName] = basePath + "/" + texFile;
        }
    }

    // Load referenced textures
    if (options.loadTextures) {
        auto loadAndAssign = [&](const std::unordered_map<std::string, std::string>& pathMap,
                                 std::function<void(Material*, int)> assign) {
            for (auto& [matName, path] : pathMap) {
                // Check if texture already loaded
                int texId = result.textures.findIdByName(path);
                if (texId < 0) {
                    Image img = loadImage(path, 4); // force RGBA
                    if (img.isValid()) {
                        texId = result.textures.addTexture(std::move(img), path);
                    }
                }
                if (texId >= 0) {
                    const Material* cm = result.materials.findByName(matName);
                    if (cm) {
                        Material* m = result.materials.getMaterial(cm->id);
                        assign(m, texId);
                    }
                }
            }
        };

        loadAndAssign(result.baseColorTexPaths,
                      [](Material* m, int id) { m->baseColorTexture = id; });
        loadAndAssign(result.normalTexPaths,
                      [](Material* m, int id) { m->normalTexture = id; });
        loadAndAssign(result.emissiveTexPaths,
                      [](Material* m, int id) { m->emissiveTexture = id; });
    }

    return result;
}

// --- OBJ parser ---

ObjLoadResult loadObj(const std::string& objFilepath, const std::string& basePath) {
    ObjLoadOptions defaultOpts;
    return loadObjWithOptions(objFilepath, defaultOpts, basePath);
}

ObjLoadResult loadObjWithOptions(const std::string& objFilepath,
                                 const ObjLoadOptions& options,
                                 const std::string& basePath) {
    ObjLoadResult result;
    std::string resolvedBase = basePath.empty() ? getDirectory(objFilepath) : basePath;

    std::ifstream file(objFilepath);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file: " + objFilepath;
        return result;
    }

    // Raw OBJ data (1-based arrays, index 0 unused)
    std::vector<Vec3> positions;
    std::vector<Vec3> texcoords;
    std::vector<Vec3> objNormals;

    // Vertex deduplication
    std::unordered_map<VertexKey, int, VertexKeyHash> vertexMap;

    int currentMaterialId = -1;
    std::string line;

    while (std::getline(file, line)) {
        line = trimWhitespace(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.push_back(Vec3(x, y, z));
        } else if (keyword == "vt") {
            float u, v;
            iss >> u >> v;
            if (options.flipUVsVertically) v = 1.0f - v;
            texcoords.push_back(Vec3(u, v, 0.0f));
        } else if (keyword == "vn") {
            float x, y, z;
            iss >> x >> y >> z;
            objNormals.push_back(Vec3(x, y, z));
        } else if (keyword == "f") {
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) tokens.push_back(tok);

            int faceVertCount = static_cast<int>(tokens.size());
            if (faceVertCount < 3) continue;

            result.geometry.faceVertexCounts.push_back(faceVertCount);
            result.geometry.faceMaterialIds.push_back(std::max(currentMaterialId, 0));

            for (auto& ftok : tokens) {
                int pi, ti, ni;
                parseFaceVertex(ftok, pi, ti, ni);

                int posIdx = resolveIndex(pi, static_cast<int>(positions.size()));
                int uvIdx = (ti != 0) ? resolveIndex(ti, static_cast<int>(texcoords.size())) : -1;
                int normIdx = (ni != 0) ? resolveIndex(ni, static_cast<int>(objNormals.size())) : -1;

                VertexKey key{posIdx, uvIdx, normIdx};
                auto it = vertexMap.find(key);
                int vertexId;

                if (it != vertexMap.end()) {
                    vertexId = it->second;
                } else {
                    vertexId = static_cast<int>(result.geometry.points.size());
                    vertexMap[key] = vertexId;

                    result.geometry.points.push_back(positions[posIdx]);

                    if (uvIdx >= 0 && uvIdx < static_cast<int>(texcoords.size())) {
                        result.geometry.uvs.push_back(texcoords[uvIdx]);
                    } else {
                        result.geometry.uvs.push_back(Vec3(0.0f, 0.0f, 0.0f));
                    }

                    if (normIdx >= 0 && normIdx < static_cast<int>(objNormals.size())) {
                        result.geometry.normals.push_back(objNormals[normIdx]);
                    } else {
                        result.geometry.normals.push_back(Vec3(0.0f, 1.0f, 0.0f));
                    }
                }

                result.geometry.faceVertexIndices.push_back(vertexId);
            }
        } else if (keyword == "mtllib") {
            std::string mtlFile;
            std::getline(iss >> std::ws, mtlFile);
            mtlFile = trimWhitespace(mtlFile);

            std::string mtlPath = resolvedBase + "/" + mtlFile;
            MtlParseResult mtlResult = parseMtlFile(mtlPath, resolvedBase, options);
            result.materials = std::move(mtlResult.materials);
            result.textures = std::move(mtlResult.textures);
        } else if (keyword == "usemtl") {
            std::string matName;
            std::getline(iss >> std::ws, matName);
            matName = trimWhitespace(matName);

            const Material* mat = result.materials.findByName(matName);
            if (mat) {
                currentMaterialId = mat->id;
            } else {
                // Material referenced but not yet loaded — assign a placeholder
                Material placeholder;
                placeholder.name = matName;
                currentMaterialId = result.materials.addMaterial(placeholder);
            }
        }
    }

    // If no materials were assigned at all, ensure a default
    if (result.materials.count() == 0) {
        Material defaultMat;
        defaultMat.name = "default";
        result.materials.addMaterial(defaultMat);
    }

    result.success = true;
    return result;
}

} // namespace light3d
