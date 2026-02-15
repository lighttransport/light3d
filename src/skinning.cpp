#include "light3d/skinning.h"
#include "light3d/math.h"
#include <cstring>

namespace light3d {

// --- CPU Skinning (LBS with 4 influences) ---

SkinnedMeshData computeSkinnedMesh(
    const MeshGeometry& geometry,
    const std::vector<Mat4>& skinningMatrices) {

    SkinnedMeshData result;
    const size_t count = geometry.vertexCount();
    result.positions.resize(count);
    result.normals.resize(count);

    const bool hasNormals = !geometry.normals.empty();

    for (size_t i = 0; i < count; ++i) {
        const Vec3& pos = geometry.points[i];
        const Vec4& idx = geometry.jointIndices[i];
        const Vec4& w = geometry.jointWeights[i];

        Vec3 skinnedPos = transformPoint(skinningMatrices[static_cast<int>(idx.x)], pos) * w.x
                        + transformPoint(skinningMatrices[static_cast<int>(idx.y)], pos) * w.y
                        + transformPoint(skinningMatrices[static_cast<int>(idx.z)], pos) * w.z
                        + transformPoint(skinningMatrices[static_cast<int>(idx.w)], pos) * w.w;
        result.positions[i] = skinnedPos;

        if (hasNormals) {
            const Vec3& nrm = geometry.normals[i];
            Vec3 skinnedNrm = transformVector(skinningMatrices[static_cast<int>(idx.x)], nrm) * w.x
                            + transformVector(skinningMatrices[static_cast<int>(idx.y)], nrm) * w.y
                            + transformVector(skinningMatrices[static_cast<int>(idx.z)], nrm) * w.z
                            + transformVector(skinningMatrices[static_cast<int>(idx.w)], nrm) * w.w;
            result.normals[i] = normalize(skinnedNrm);
        }
    }

    return result;
}

void applyCPUSkinning(
    MeshGeometry& geometry,
    const std::vector<Mat4>& skinningMatrices) {

    auto result = computeSkinnedMesh(geometry, skinningMatrices);
    geometry.points = std::move(result.positions);
    if (!geometry.normals.empty()) {
        geometry.normals = std::move(result.normals);
    }
}

// --- GPU Vertex Buffer Building ---

SkinnedVertexLayout getSkinnedVertexLayout() {
    SkinnedVertexLayout layout;
    layout.stride       = kSkinnedVertexFloats * static_cast<int>(sizeof(float));  // 76
    layout.posOffset    = 0;
    layout.colorOffset  = 3 * static_cast<int>(sizeof(float));   // 12
    layout.normalOffset = 6 * static_cast<int>(sizeof(float));   // 24
    layout.uvOffset     = 9 * static_cast<int>(sizeof(float));   // 36
    layout.jointOffset  = 11 * static_cast<int>(sizeof(float));  // 44
    layout.weightOffset = 15 * static_cast<int>(sizeof(float));  // 60
    return layout;
}

std::vector<float> buildSkinnedVertexBuffer(const MeshGeometry& geometry) {
    const size_t count = geometry.vertexCount();
    std::vector<float> buf(count * kSkinnedVertexFloats);

    const bool hasNormals = !geometry.normals.empty();
    const bool hasUVs = !geometry.uvs.empty();
    const bool hasSkinning = geometry.hasSkinning();

    for (size_t i = 0; i < count; ++i) {
        float* v = &buf[i * kSkinnedVertexFloats];

        // Position (3)
        v[0] = geometry.points[i].x;
        v[1] = geometry.points[i].y;
        v[2] = geometry.points[i].z;

        // Color (3) — default white
        v[3] = 1.0f;
        v[4] = 1.0f;
        v[5] = 1.0f;

        // Normal (3)
        if (hasNormals) {
            v[6] = geometry.normals[i].x;
            v[7] = geometry.normals[i].y;
            v[8] = geometry.normals[i].z;
        } else {
            v[6] = 0.0f;
            v[7] = 1.0f;
            v[8] = 0.0f;
        }

        // UV (2)
        if (hasUVs) {
            v[9]  = geometry.uvs[i].x;
            v[10] = geometry.uvs[i].y;
        } else {
            v[9]  = 0.0f;
            v[10] = 0.0f;
        }

        // Joint indices (4)
        if (hasSkinning) {
            v[11] = geometry.jointIndices[i].x;
            v[12] = geometry.jointIndices[i].y;
            v[13] = geometry.jointIndices[i].z;
            v[14] = geometry.jointIndices[i].w;
        } else {
            v[11] = 0.0f;
            v[12] = 0.0f;
            v[13] = 0.0f;
            v[14] = 0.0f;
        }

        // Joint weights (4)
        if (hasSkinning) {
            v[15] = geometry.jointWeights[i].x;
            v[16] = geometry.jointWeights[i].y;
            v[17] = geometry.jointWeights[i].z;
            v[18] = geometry.jointWeights[i].w;
        } else {
            v[15] = 1.0f;
            v[16] = 0.0f;
            v[17] = 0.0f;
            v[18] = 0.0f;
        }
    }

    return buf;
}

// --- Bone Matrix Packing ---

std::vector<float> packBoneMatricesToBuffer(
    const std::vector<Mat4>& skinningMatrices,
    int maxBones) {

    std::vector<float> buf(static_cast<size_t>(maxBones) * 16, 0.0f);

    const size_t count = skinningMatrices.size();
    for (size_t i = 0; i < count && i < static_cast<size_t>(maxBones); ++i) {
        std::memcpy(&buf[i * 16], skinningMatrices[i].data(), 16 * sizeof(float));
    }

    // Remaining slots are already zero-filled; set them to identity
    for (size_t i = count; i < static_cast<size_t>(maxBones); ++i) {
        buf[i * 16 + 0]  = 1.0f;  // m[0]  = col0.x
        buf[i * 16 + 5]  = 1.0f;  // m[5]  = col1.y
        buf[i * 16 + 10] = 1.0f;  // m[10] = col2.z
        buf[i * 16 + 15] = 1.0f;  // m[15] = col3.w
    }

    return buf;
}

BoneTextureData packBoneMatricesToTexture(
    const std::vector<Mat4>& skinningMatrices,
    int maxBones) {

    BoneTextureData data;
    data.width = 4;
    data.height = maxBones;
    // Each row = 4 RGBA texels = 16 floats = one mat4
    data.pixels.resize(static_cast<size_t>(4) * maxBones * 4, 0.0f);

    const size_t count = skinningMatrices.size();
    for (size_t i = 0; i < count && i < static_cast<size_t>(maxBones); ++i) {
        // Row i: 4 texels, each texel = 4 floats (RGBA)
        // texel 0 = column 0 of matrix, texel 1 = column 1, etc.
        std::memcpy(&data.pixels[i * 16], skinningMatrices[i].data(), 16 * sizeof(float));
    }

    // Remaining slots: identity matrices
    for (size_t i = count; i < static_cast<size_t>(maxBones); ++i) {
        data.pixels[i * 16 + 0]  = 1.0f;
        data.pixels[i * 16 + 5]  = 1.0f;
        data.pixels[i * 16 + 10] = 1.0f;
        data.pixels[i * 16 + 15] = 1.0f;
    }

    return data;
}

// --- Shader Source Strings ---

// GL 330 core: bone matrices via RGBA32F texture
const char* getSkinningVertexShaderGL330_Texture() {
    return R"(#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aJointIndices;
layout(location=5) in vec4 aJointWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform sampler2D uBoneTexture;
uniform int uBoneCount;

out vec3 vColor;
out vec3 vNormal;
out vec2 vUV;
out vec3 vWorldPos;

mat4 getBoneMatrix(int idx) {
    return mat4(
        texelFetch(uBoneTexture, ivec2(0, idx), 0),
        texelFetch(uBoneTexture, ivec2(1, idx), 0),
        texelFetch(uBoneTexture, ivec2(2, idx), 0),
        texelFetch(uBoneTexture, ivec2(3, idx), 0)
    );
}

void main() {
    mat4 skinMat = getBoneMatrix(int(aJointIndices.x)) * aJointWeights.x
                 + getBoneMatrix(int(aJointIndices.y)) * aJointWeights.y
                 + getBoneMatrix(int(aJointIndices.z)) * aJointWeights.z
                 + getBoneMatrix(int(aJointIndices.w)) * aJointWeights.w;

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec3 skinnedNrm = normalize(mat3(skinMat) * aNormal);

    vec4 worldPos = uModel * skinnedPos;
    gl_Position = uViewProj * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * skinnedNrm;
    vColor = aColor;
    vUV = aUV;
}
)";
}

// GL 330 fragment shader shared by all GL skinning variants
const char* getSkinningFragmentShaderGL330() {
    return R"(#version 330 core

in vec3 vColor;
in vec3 vNormal;
in vec2 vUV;
in vec3 vWorldPos;

out vec4 fragColor;

uniform vec3 uLightDir;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient = 0.15 * vColor;
    vec3 diffuse = NdotL * vColor;

    fragColor = vec4(ambient + diffuse, 1.0);
}
)";
}

// GL 330 core: bone matrices via UBO (std140)
const char* getSkinningVertexShaderGL330_UBO() {
    return R"(#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aJointIndices;
layout(location=5) in vec4 aJointWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;

layout(std140) uniform BoneMatrices {
    mat4 bones[128];
};

out vec3 vColor;
out vec3 vNormal;
out vec2 vUV;
out vec3 vWorldPos;

void main() {
    mat4 skinMat = bones[int(aJointIndices.x)] * aJointWeights.x
                 + bones[int(aJointIndices.y)] * aJointWeights.y
                 + bones[int(aJointIndices.z)] * aJointWeights.z
                 + bones[int(aJointIndices.w)] * aJointWeights.w;

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec3 skinnedNrm = normalize(mat3(skinMat) * aNormal);

    vec4 worldPos = uModel * skinnedPos;
    gl_Position = uViewProj * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * skinnedNrm;
    vColor = aColor;
    vUV = aUV;
}
)";
}

// GL 430 core: bone matrices via SSBO (std430)
const char* getSkinningVertexShaderGL430_SSBO() {
    return R"(#version 430 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aJointIndices;
layout(location=5) in vec4 aJointWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;

layout(std430, binding=0) readonly buffer BoneMatrices {
    mat4 bones[];
};

out vec3 vColor;
out vec3 vNormal;
out vec2 vUV;
out vec3 vWorldPos;

void main() {
    mat4 skinMat = bones[int(aJointIndices.x)] * aJointWeights.x
                 + bones[int(aJointIndices.y)] * aJointWeights.y
                 + bones[int(aJointIndices.z)] * aJointWeights.z
                 + bones[int(aJointIndices.w)] * aJointWeights.w;

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec3 skinnedNrm = normalize(mat3(skinMat) * aNormal);

    vec4 worldPos = uModel * skinnedPos;
    gl_Position = uViewProj * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * skinnedNrm;
    vColor = aColor;
    vUV = aUV;
}
)";
}

// Vulkan 450: UBO variant with push constants for model/viewProj
const char* getSkinningVertexShaderVK450_UBO() {
    return R"(#version 450

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aJointIndices;
layout(location=5) in vec4 aJointWeights;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 viewProj;
} pc;

layout(set=0, binding=0) uniform BoneMatrices {
    mat4 bones[128];
};

layout(location=0) out vec3 vColor;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV;
layout(location=3) out vec3 vWorldPos;

void main() {
    mat4 skinMat = bones[int(aJointIndices.x)] * aJointWeights.x
                 + bones[int(aJointIndices.y)] * aJointWeights.y
                 + bones[int(aJointIndices.z)] * aJointWeights.z
                 + bones[int(aJointIndices.w)] * aJointWeights.w;

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec3 skinnedNrm = normalize(mat3(skinMat) * aNormal);

    vec4 worldPos = pc.model * skinnedPos;
    gl_Position = pc.viewProj * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = mat3(pc.model) * skinnedNrm;
    vColor = aColor;
    vUV = aUV;
}
)";
}

// Vulkan 450: SSBO variant with push constants for model/viewProj
const char* getSkinningVertexShaderVK450_SSBO() {
    return R"(#version 450

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aJointIndices;
layout(location=5) in vec4 aJointWeights;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 viewProj;
} pc;

layout(set=0, binding=0) readonly buffer BoneMatrices {
    mat4 bones[];
};

layout(location=0) out vec3 vColor;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV;
layout(location=3) out vec3 vWorldPos;

void main() {
    mat4 skinMat = bones[int(aJointIndices.x)] * aJointWeights.x
                 + bones[int(aJointIndices.y)] * aJointWeights.y
                 + bones[int(aJointIndices.z)] * aJointWeights.z
                 + bones[int(aJointIndices.w)] * aJointWeights.w;

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    vec3 skinnedNrm = normalize(mat3(skinMat) * aNormal);

    vec4 worldPos = pc.model * skinnedPos;
    gl_Position = pc.viewProj * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = mat3(pc.model) * skinnedNrm;
    vColor = aColor;
    vUV = aUV;
}
)";
}

} // namespace light3d
