#include "light3d/material.h"
#include "light3d/mesh_data.h"
#include <algorithm>
#include <cassert>

namespace light3d {

// --- MaterialLibrary ---

int MaterialLibrary::addMaterial(Material material) {
    int id = static_cast<int>(materials_.size());
    material.id = id;
    if (!material.name.empty()) {
        nameToIndex_[material.name] = id;
    }
    materials_.push_back(std::move(material));
    return id;
}

const Material* MaterialLibrary::getMaterial(int id) const {
    if (id < 0 || id >= static_cast<int>(materials_.size())) return nullptr;
    return &materials_[id];
}

Material* MaterialLibrary::getMaterial(int id) {
    if (id < 0 || id >= static_cast<int>(materials_.size())) return nullptr;
    return &materials_[id];
}

const Material* MaterialLibrary::findByName(const std::string& name) const {
    auto it = nameToIndex_.find(name);
    if (it == nameToIndex_.end()) return nullptr;
    return &materials_[it->second];
}

// --- Triangulation helper ---

// Fan-triangulate a single face, appending triangle vertex indices to `out`.
// For a face with vertices [v0, v1, v2, ..., vN-1], produces triangles:
//   (v0, v1, v2), (v0, v2, v3), ..., (v0, vN-2, vN-1)
static void triangulateFace(const int* faceIndices, int vertexCount,
                            std::vector<uint32_t>& out) {
    for (int i = 1; i + 1 < vertexCount; ++i) {
        out.push_back(static_cast<uint32_t>(faceIndices[0]));
        out.push_back(static_cast<uint32_t>(faceIndices[i]));
        out.push_back(static_cast<uint32_t>(faceIndices[i + 1]));
    }
}

// --- buildSubmeshes ---

SubmeshData buildSubmeshes(const MeshGeometry& geometry) {
    struct TriangleRecord {
        int materialId;
        uint32_t idx[3];
    };

    std::vector<TriangleRecord> triangles;
    bool hasMatIds = geometry.hasPerFaceMaterials();

    int offset = 0;
    for (size_t face = 0; face < geometry.faceVertexCounts.size(); ++face) {
        int count = geometry.faceVertexCounts[face];
        int matId = hasMatIds ? geometry.faceMaterialIds[face] : 0;
        const int* faceVerts = &geometry.faceVertexIndices[offset];

        for (int i = 1; i + 1 < count; ++i) {
            TriangleRecord rec;
            rec.materialId = matId;
            rec.idx[0] = static_cast<uint32_t>(faceVerts[0]);
            rec.idx[1] = static_cast<uint32_t>(faceVerts[i]);
            rec.idx[2] = static_cast<uint32_t>(faceVerts[i + 1]);
            triangles.push_back(rec);
        }

        offset += count;
    }

    // Stable sort by material ID (preserves face order within each material group)
    std::stable_sort(triangles.begin(), triangles.end(),
                     [](const TriangleRecord& a, const TriangleRecord& b) {
                         return a.materialId < b.materialId;
                     });

    // Build output
    SubmeshData result;
    result.triangleIndices.reserve(triangles.size() * 3);

    if (triangles.empty()) return result;

    int currentMatId = triangles[0].materialId;
    int groupStart = 0;

    for (size_t i = 0; i < triangles.size(); ++i) {
        result.triangleIndices.push_back(triangles[i].idx[0]);
        result.triangleIndices.push_back(triangles[i].idx[1]);
        result.triangleIndices.push_back(triangles[i].idx[2]);

        bool last = (i + 1 == triangles.size());
        bool matChange = !last && (triangles[i + 1].materialId != currentMatId);

        if (last || matChange) {
            Submesh sub;
            sub.materialId = currentMatId;
            sub.triangleOffset = groupStart;
            sub.triangleCount = static_cast<int>(i) - groupStart + 1;
            result.submeshes.push_back(sub);

            if (matChange) {
                currentMatId = triangles[i + 1].materialId;
                groupStart = static_cast<int>(i) + 1;
            }
        }
    }

    return result;
}

// --- packMaterialsToBuffer ---

std::vector<float> packMaterialsToBuffer(const MaterialLibrary& library) {
    std::vector<float> buf;
    buf.reserve(library.count() * kPackedMaterialFloats);

    for (const auto& mat : library.materials()) {
        // vec4(baseColor.rgb, metallic)
        buf.push_back(mat.baseColor.x);
        buf.push_back(mat.baseColor.y);
        buf.push_back(mat.baseColor.z);
        buf.push_back(mat.metallic);

        // vec4(emissive.rgb, roughness)
        buf.push_back(mat.emissive.x);
        buf.push_back(mat.emissive.y);
        buf.push_back(mat.emissive.z);
        buf.push_back(mat.roughness);

        // vec4(alpha, alphaCutoff, doubleSided, 0)
        buf.push_back(mat.alpha);
        buf.push_back(mat.alphaCutoff);
        buf.push_back(mat.doubleSided ? 1.0f : 0.0f);
        buf.push_back(0.0f);
    }

    return buf;
}

// --- buildTriangleMaterialIds ---

TriangleMaterialIdData buildTriangleMaterialIds(const MeshGeometry& geometry) {
    TriangleMaterialIdData result;
    bool hasMatIds = geometry.hasPerFaceMaterials();

    int offset = 0;
    for (size_t face = 0; face < geometry.faceVertexCounts.size(); ++face) {
        int count = geometry.faceVertexCounts[face];
        int matId = hasMatIds ? geometry.faceMaterialIds[face] : 0;
        const int* faceVerts = &geometry.faceVertexIndices[offset];

        for (int i = 1; i + 1 < count; ++i) {
            result.triangleIndices.push_back(static_cast<uint32_t>(faceVerts[0]));
            result.triangleIndices.push_back(static_cast<uint32_t>(faceVerts[i]));
            result.triangleIndices.push_back(static_cast<uint32_t>(faceVerts[i + 1]));
            result.materialIds.push_back(static_cast<uint32_t>(matId));
        }

        offset += count;
    }

    return result;
}

// --- Shader Source Strings ---

// ==================== GL330 Submesh Shaders ====================

const char* getMaterialVertexShaderGL330() {
    return R"glsl(#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModelViewProj;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    gl_Position = uModelViewProj * vec4(aPosition, 1.0);
}
)glsl";
}

const char* getMaterialFragmentShaderGL330() {
    return R"glsl(#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

// Material uniforms (one draw call per submesh)
uniform vec3 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmissive;
uniform float uAlpha;

uniform vec3 uCameraPos;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Simple directional light
    vec3 L = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(N, L), 0.0);

    // Simple PBR-ish shading
    vec3 diffuse = uBaseColor * (1.0 - uMetallic) * NdotL;

    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specPower = mix(16.0, 256.0, 1.0 - uRoughness);
    vec3 specColor = mix(vec3(0.04), uBaseColor, uMetallic);
    vec3 specular = specColor * pow(NdotH, specPower);

    // Ambient
    vec3 ambient = uBaseColor * 0.05;

    vec3 color = ambient + diffuse + specular + uEmissive;
    fragColor = vec4(color, uAlpha);
}
)glsl";
}

// ==================== GL430 Bindless Shaders ====================

const char* getMaterialVertexShaderGL430() {
    return R"glsl(#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModelViewProj;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    gl_Position = uModelViewProj * vec4(aPosition, 1.0);
}
)glsl";
}

const char* getMaterialFragmentShaderGL430() {
    return R"glsl(#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;

// Materials SSBO: each material is 3 x vec4 (12 floats)
layout(std430, binding = 1) readonly buffer Materials {
    vec4 materials[];
};

// Per-triangle material ID
layout(std430, binding = 2) readonly buffer FaceMaterialIds {
    uint faceMaterialId[];
};

uniform vec3 uCameraPos;

out vec4 fragColor;

void main() {
    int matId = int(faceMaterialId[gl_PrimitiveID]);
    int base = matId * 3;

    vec3 baseColor = materials[base + 0].xyz;
    float metallic = materials[base + 0].w;
    vec3 emissive  = materials[base + 1].xyz;
    float roughness = materials[base + 1].w;
    float alpha     = materials[base + 2].x;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Simple directional light
    vec3 L = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(N, L), 0.0);

    // Simple PBR-ish shading
    vec3 diffuse = baseColor * (1.0 - metallic) * NdotL;

    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specPower = mix(16.0, 256.0, 1.0 - roughness);
    vec3 specColor = mix(vec3(0.04), baseColor, metallic);
    vec3 specular = specColor * pow(NdotH, specPower);

    // Ambient
    vec3 ambient = baseColor * 0.05;

    vec3 color = ambient + diffuse + specular + emissive;
    fragColor = vec4(color, alpha);
}
)glsl";
}

// ==================== Vulkan 450 Shaders ====================

const char* getMaterialVertexShaderVK450() {
    return R"glsl(#version 450

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(push_constant) uniform PushConstants {
    mat4 modelViewProj;
    mat4 model;
} pc;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;

void main() {
    vec4 worldPos = pc.model * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    // Approximate normal matrix (works for uniform scale)
    vNormal = normalize(mat3(pc.model) * aNormal);
    gl_Position = pc.modelViewProj * vec4(aPosition, 1.0);
}
)glsl";
}

const char* getMaterialFragmentShaderVK450() {
    return R"glsl(#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;

// Materials SSBO: each material is 3 x vec4 (12 floats)
layout(set = 0, binding = 1) readonly buffer Materials {
    vec4 materials[];
};

// Per-triangle material ID
layout(set = 0, binding = 2) readonly buffer FaceMaterialIds {
    uint faceMaterialId[];
};

layout(set = 0, binding = 0) uniform SceneUBO {
    vec3 cameraPos;
};

layout(location = 0) out vec4 fragColor;

void main() {
    int matId = int(faceMaterialId[gl_PrimitiveID]);
    int base = matId * 3;

    vec3 baseColor = materials[base + 0].xyz;
    float metallic = materials[base + 0].w;
    vec3 emissive  = materials[base + 1].xyz;
    float roughness = materials[base + 1].w;
    float alpha     = materials[base + 2].x;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(cameraPos - vWorldPos);

    // Simple directional light
    vec3 L = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(N, L), 0.0);

    // Simple PBR-ish shading
    vec3 diffuse = baseColor * (1.0 - metallic) * NdotL;

    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specPower = mix(16.0, 256.0, 1.0 - roughness);
    vec3 specColor = mix(vec3(0.04), baseColor, metallic);
    vec3 specular = specColor * pow(NdotH, specPower);

    // Ambient
    vec3 ambient = baseColor * 0.05;

    vec3 color = ambient + diffuse + specular + emissive;
    fragColor = vec4(color, alpha);
}
)glsl";
}

} // namespace light3d
