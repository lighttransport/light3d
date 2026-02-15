#pragma once

#include "mesh_data.h"
#include <vector>

namespace light3d {

constexpr int kMaxBones = 128;

enum class SkinningMethod {
    CPU,            // Software skinning on CPU
    BoneTexture,    // GPU: bone matrices in RGBA32F texture (WebGL/GL3)
    UBO,            // GPU: bone matrices in uniform buffer (GL3.1+)
    SSBO            // GPU: bone matrices in storage buffer (GL4.3+/WebGPU)
};

// --- CPU Skinning ---

struct SkinnedMeshData {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
};

// Non-destructive: produces deformed positions/normals from original geometry
SkinnedMeshData computeSkinnedMesh(
    const MeshGeometry& geometry,
    const std::vector<Mat4>& skinningMatrices);

// In-place: deforms geometry.points and geometry.normals directly
void applyCPUSkinning(
    MeshGeometry& geometry,
    const std::vector<Mat4>& skinningMatrices);

// --- GPU Vertex Buffer Building ---

// Skinned vertex: pos(3)+color(3)+normal(3)+uv(2)+joints(4)+weights(4) = 19 floats
constexpr int kSkinnedVertexFloats = 19;

struct SkinnedVertexLayout {
    int stride;        // 19 * sizeof(float) = 76
    int posOffset;     // 0
    int colorOffset;   // 12
    int normalOffset;  // 24
    int uvOffset;      // 36
    int jointOffset;   // 44
    int weightOffset;  // 60
};

SkinnedVertexLayout getSkinnedVertexLayout();

// Build interleaved vertex buffer from MeshGeometry (for GPU skinning)
std::vector<float> buildSkinnedVertexBuffer(const MeshGeometry& geometry);

// --- Bone Matrix Packing ---

// Pack for UBO/SSBO: flat array of 16 floats per mat4, padded to maxBones
std::vector<float> packBoneMatricesToBuffer(
    const std::vector<Mat4>& skinningMatrices,
    int maxBones = kMaxBones);

// Pack for texture: same data, but returned with texture dimensions
// Layout: width=4, height=maxBones. Each row = 4 RGBA32F texels = one mat4
struct BoneTextureData {
    std::vector<float> pixels;  // 4 * maxBones * 4 floats
    int width;   // 4
    int height;  // maxBones
};

BoneTextureData packBoneMatricesToTexture(
    const std::vector<Mat4>& skinningMatrices,
    int maxBones = kMaxBones);

// --- Shader Source Strings ---

// GL 330 core: bone matrices via RGBA32F texture
const char* getSkinningVertexShaderGL330_Texture();
const char* getSkinningFragmentShaderGL330();

// GL 330 core: bone matrices via UBO (std140, binding=0)
const char* getSkinningVertexShaderGL330_UBO();

// GL 430 core: bone matrices via SSBO (std430, binding=0)
const char* getSkinningVertexShaderGL430_SSBO();

// Vulkan 450: UBO variant (set=0, binding=0)
const char* getSkinningVertexShaderVK450_UBO();

// Vulkan 450: SSBO variant (set=0, binding=0)
const char* getSkinningVertexShaderVK450_SSBO();

} // namespace light3d
