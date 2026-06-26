/*
 * binding.cc — Embind bindings for lightusd-c mesh extraction.
 *
 * Exposes a single high-level function to JavaScript:
 *   loadUsdMeshes(dataPtr, dataSize, format) → vector<UsdMesh>
 *
 * Each UsdMesh contains interleaved Float32Array vertex data
 * (pos3 + normal3 + uv2 = 8 floats per vertex) ready for WebGPU.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "wasm_log.h"

extern "C" {
#include <lightusd/lightusd-c.h>
#include <lightusd/lusd_instance.h>
#include <lightusd/lusd_stage.h>
#include <lightusd/lusd_prim.h>
#include <lightusd/lusd_attribute.h>
#include <lightusd/lusd_relationship.h>
#include <lightusd/lusd_path.h>
#include <lightusd/lusd_value.h>
#include <lightusd/lusd_material.h>
#include <lydra_c_mesh.h>
}

namespace {

struct UsdMesh {
    std::string name;
    // Interleaved: pos(3) + normal(3) + uv(2) = 8 floats per vertex
    std::vector<float> vertices;
    uint32_t vertexCount;
    // Bounding box
    float bboxMin[3];
    float bboxMax[3];

    // --- Skinning (parallel to vertices; top-4 influences per vertex) ---
    bool hasSkin = false;
    int  influences = 0;            // original influences-per-vertex (elementSize)
    std::vector<float> jointIndices; // vertexCount * 4 (as float, matches WGSL skin layout)
    std::vector<float> jointWeights; // vertexCount * 4 (renormalized over top-4)
    std::string skeletonPath;

    // --- PBR material (resolved via material:binding → UsdPreviewSurface) ---
    bool  hasMaterial = false;
    float baseColor[3] = {0.8f, 0.8f, 0.8f};
    float metallic     = 0.0f;
    float roughness    = 0.5f;
    float emissive[3]  = {0.0f, 0.0f, 0.0f};
    std::string materialPath;
};

// Resolved PBR material from the stage-level material API (no layer required).
struct StageMaterial {
    std::string path;
    float baseColor[3];
    float metallic;
    float roughness;
    float emissive[3];
};

void normalize3(float* v) {
    float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-8f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

void crossProduct(const float* a, const float* b, float* out) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

void extractMeshesRecursive(LusdPrim prim, std::vector<UsdMesh>& meshes,
                            LusdStage stage, LusdInstance instance,
                            const std::vector<StageMaterial>& stageMaterials) {
    const char* typeName = lusdPrimGetTypeName(prim);

    if (typeName && strcmp(typeName, "Mesh") == 0) {
        LusdValue val;

        // points (required)
        if (lusdPrimGetAttributeDefault(prim, "points", &val) != LUSD_SUCCESS) goto children;
        uint64_t ptCount;
        const LusdFloat3* pts;
        if (lusdValueGetArrayPtrFloat3(val, &ptCount, &pts) != LUSD_SUCCESS) goto children;

        // faceVertexCounts (required)
        if (lusdPrimGetAttributeDefault(prim, "faceVertexCounts", &val) != LUSD_SUCCESS) goto children;
        uint64_t fvcCount;
        const int32_t* fvcs;
        if (lusdValueGetArrayPtrInt32(val, &fvcCount, &fvcs) != LUSD_SUCCESS) goto children;

        // faceVertexIndices (required)
        if (lusdPrimGetAttributeDefault(prim, "faceVertexIndices", &val) != LUSD_SUCCESS) goto children;
        uint64_t fviCount;
        const int32_t* fvis;
        if (lusdValueGetArrayPtrInt32(val, &fviCount, &fvis) != LUSD_SUCCESS) goto children;

        {
            // normals (optional)
            const LusdFloat3* normals = nullptr;
            uint64_t normalCount = 0;
            if (lusdPrimGetAttributeDefault(prim, "normals", &val) == LUSD_SUCCESS) {
                lusdValueGetArrayPtrFloat3(val, &normalCount, &normals);
            }

            // uvs (optional)
            const LusdFloat2* uvs = nullptr;
            uint64_t uvCount = 0;
            if (lusdPrimGetAttributeDefault(prim, "primvars:st", &val) == LUSD_SUCCESS) {
                lusdValueGetArrayPtrFloat2(val, &uvCount, &uvs);
            }

            // skinning primvars (optional): per-vertex jointIndices/jointWeights,
            // elementSize = influences-per-vertex inferred from count / pointCount.
            const int32_t* jiPtr = nullptr; uint64_t jiCount = 0;
            const float*   jwPtr = nullptr; uint64_t jwCount = 0;
            if (lusdPrimGetAttributeDefault(prim, "primvars:skel:jointIndices", &val) == LUSD_SUCCESS) {
                lusdValueGetArrayPtrInt32(val, &jiCount, &jiPtr);
            }
            if (lusdPrimGetAttributeDefault(prim, "primvars:skel:jointWeights", &val) == LUSD_SUCCESS) {
                lusdValueGetArrayPtrFloat(val, &jwCount, &jwPtr);
            }
            // NOTE: as of this lightusd-c build, the UsdSkel primvars are parsed into
            // tinyusdz's SkelBindingAPI and are NOT surfaced through the generic attribute
            // API (lusdPrimGetAttributeDefault returns NOT_FOUND for primvars:skel:*).
            // The packing below therefore stays inert (hasSkin=false) until lightusd-c
            // exposes skel data; the JS-side `skin` object and the WGSL skin layout are
            // already wired so this lights up with no viewer changes once the API lands.
            int influences = 0;
            if (jiPtr && jwPtr && ptCount > 0 && jiCount == jwCount && (jiCount % ptCount) == 0) {
                influences = (int)(jiCount / ptCount);
            }
            const bool hasSkin = influences > 0;

            // Build triangle soup directly (same as glview)
            UsdMesh mesh;
            mesh.name = lusdPrimGetName(prim) ? lusdPrimGetName(prim) : "unnamed";
            mesh.hasSkin = hasSkin;
            mesh.influences = influences;
            mesh.bboxMin[0] = mesh.bboxMin[1] = mesh.bboxMin[2] =  1e30f;
            mesh.bboxMax[0] = mesh.bboxMax[1] = mesh.bboxMax[2] = -1e30f;

            uint32_t totalTris = 0;
            for (uint64_t f = 0; f < fvcCount; ++f) {
                if (fvcs[f] >= 3) totalTris += (uint32_t)(fvcs[f] - 2);
            }
            mesh.vertices.reserve(totalTris * 3 * 8);
            if (hasSkin) {
                mesh.jointIndices.reserve(totalTris * 3 * 4);
                mesh.jointWeights.reserve(totalTris * 3 * 4);
            }

            int fvOffset = 0;
            for (uint64_t f = 0; f < fvcCount; ++f) {
                int fvc = fvcs[f];
                for (int v = 1; v < fvc - 1; ++v) {
                    int i0 = fvOffset;
                    int i1 = fvOffset + v;
                    int i2 = fvOffset + v + 1;

                    int vIdx[3] = { fvis[i0], fvis[i1], fvis[i2] };
                    int fvIdx[3] = { i0, i1, i2 };

                    // Compute face normal for fallback
                    float faceNormal[3] = {0, 1, 0};
                    if (!normals) {
                        float e1[3] = {
                            pts[vIdx[1]].x - pts[vIdx[0]].x,
                            pts[vIdx[1]].y - pts[vIdx[0]].y,
                            pts[vIdx[1]].z - pts[vIdx[0]].z
                        };
                        float e2[3] = {
                            pts[vIdx[2]].x - pts[vIdx[0]].x,
                            pts[vIdx[2]].y - pts[vIdx[0]].y,
                            pts[vIdx[2]].z - pts[vIdx[0]].z
                        };
                        crossProduct(e1, e2, faceNormal);
                        normalize3(faceNormal);
                    }

                    for (int k = 0; k < 3; ++k) {
                        int vi = vIdx[k];
                        int fvi_idx = fvIdx[k];

                        float px = pts[vi].x;
                        float py = pts[vi].y;
                        float pz = pts[vi].z;

                        // Update bbox
                        if (px < mesh.bboxMin[0]) mesh.bboxMin[0] = px;
                        if (py < mesh.bboxMin[1]) mesh.bboxMin[1] = py;
                        if (pz < mesh.bboxMin[2]) mesh.bboxMin[2] = pz;
                        if (px > mesh.bboxMax[0]) mesh.bboxMax[0] = px;
                        if (py > mesh.bboxMax[1]) mesh.bboxMax[1] = py;
                        if (pz > mesh.bboxMax[2]) mesh.bboxMax[2] = pz;

                        float nx, ny, nz;
                        if (normals) {
                            int nIdx = (normalCount == ptCount) ? vi : fvi_idx;
                            if (nIdx < (int)normalCount) {
                                nx = normals[nIdx].x; ny = normals[nIdx].y; nz = normals[nIdx].z;
                            } else {
                                nx = 0; ny = 1; nz = 0;
                            }
                        } else {
                            nx = faceNormal[0]; ny = faceNormal[1]; nz = faceNormal[2];
                        }

                        float u = 0, uv_v = 0;
                        if (uvs) {
                            int uIdx = (uvCount == ptCount) ? vi : fvi_idx;
                            if (uIdx < (int)uvCount) {
                                u = uvs[uIdx].x; uv_v = uvs[uIdx].y;
                            }
                        }

                        mesh.vertices.push_back(px);
                        mesh.vertices.push_back(py);
                        mesh.vertices.push_back(pz);
                        mesh.vertices.push_back(nx);
                        mesh.vertices.push_back(ny);
                        mesh.vertices.push_back(nz);
                        mesh.vertices.push_back(u);
                        mesh.vertices.push_back(uv_v);

                        if (hasSkin) {
                            // Gather this point's influences, keep the top-4 by weight, renormalize.
                            float w4[4] = {0, 0, 0, 0};
                            float j4[4] = {0, 0, 0, 0};
                            const int base = vi * influences;
                            for (int e = 0; e < influences; ++e) {
                                const float w = jwPtr[base + e];
                                int minIdx = 0;
                                for (int t = 1; t < 4; ++t) if (w4[t] < w4[minIdx]) minIdx = t;
                                if (w > w4[minIdx]) { w4[minIdx] = w; j4[minIdx] = (float)jiPtr[base + e]; }
                            }
                            const float sum = w4[0] + w4[1] + w4[2] + w4[3];
                            if (sum > 1e-8f) for (int t = 0; t < 4; ++t) w4[t] /= sum;
                            for (int t = 0; t < 4; ++t) { mesh.jointIndices.push_back(j4[t]); mesh.jointWeights.push_back(w4[t]); }
                        }
                    }
                }
                fvOffset += fvc;
            }

            // skeleton binding path (informational)
            if (hasSkin) {
                uint32_t skelCount = 0;
                if (lusdPrimGetRelationshipTargetCount(prim, "skel:skeleton", &skelCount) == LUSD_SUCCESS && skelCount > 0) {
                    LusdPath sp;
                    if (lusdPrimGetRelationshipTargets(prim, "skel:skeleton", 1, &sp) == LUSD_SUCCESS) {
                        const char* t = lusdPathGetText(sp);
                        if (t) mesh.skeletonPath = t;
                    }
                }
            }

            // PBR material. Preferred: resolve the mesh's material:binding relationship to a
            // path and match the stage material table. NOTE: this lightusd-c build returns
            // FEATURE_NOT_PRESENT for relationship/layer APIs, so as a fallback a single-material
            // stage is applied to every mesh (covers the common single-material asset case).
            auto applyStageMaterial = [&](const StageMaterial& sm) {
                mesh.hasMaterial = true;
                mesh.baseColor[0] = sm.baseColor[0]; mesh.baseColor[1] = sm.baseColor[1]; mesh.baseColor[2] = sm.baseColor[2];
                mesh.metallic = sm.metallic; mesh.roughness = sm.roughness;
                mesh.emissive[0] = sm.emissive[0]; mesh.emissive[1] = sm.emissive[1]; mesh.emissive[2] = sm.emissive[2];
                mesh.materialPath = sm.path;
            };
            if (!stageMaterials.empty()) {
                uint32_t bindCount = 0;
                LusdPath bp;
                if (lusdPrimGetRelationshipTargetCount(prim, "material:binding", &bindCount) == LUSD_SUCCESS &&
                    bindCount > 0 &&
                    lusdPrimGetRelationshipTargets(prim, "material:binding", 1, &bp) == LUSD_SUCCESS) {
                    const char* bptext = lusdPathGetText(bp);
                    if (bptext) {
                        for (const auto& sm : stageMaterials) {
                            if (sm.path == bptext) { applyStageMaterial(sm); break; }
                        }
                    }
                }
                // Fallback: single material on the stage → apply to all meshes.
                if (!mesh.hasMaterial && stageMaterials.size() == 1) {
                    applyStageMaterial(stageMaterials[0]);
                }
            }
            // end material binding

            mesh.vertexCount = (uint32_t)(mesh.vertices.size() / 8);
            if (mesh.vertexCount > 0) {
                meshes.push_back(std::move(mesh));
            }
        }
    }

children:
    uint32_t childCount = 0;
    lusdPrimGetChildCount(prim, &childCount);
    if (childCount > 0) {
        std::vector<LusdPrim> children(childCount);
        lusdPrimGetChildren(prim, childCount, children.data());
        for (uint32_t i = 0; i < childCount; ++i) {
            extractMeshesRecursive(children[i], meshes, stage, instance, stageMaterials);
        }
    }
}

} // anonymous namespace


// --- Embind bindings ---

emscripten::val loadUsdFromMemory(emscripten::val jsData, int format) {
    using namespace emscripten;

    // Convert JS Uint8Array to C++ vector
    auto length = jsData["length"].as<unsigned>();
    std::vector<uint8_t> data(length);
    val memoryView = val(typed_memory_view(length, data.data()));
    memoryView.call<void>("set", jsData);

    LUSD_LOG("[lightusd-wasm] Loading %u bytes, format=%d\n", length, format);

    // Create lightusd instance
    LusdInstanceCreateInfo instCI = {};
    instCI.sType = LUSD_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instCI.apiVersion = LUSD_API_VERSION;

    LusdInstance instance = nullptr;
    LusdResult res = lusdCreateInstance(&instCI, nullptr, &instance);
    if (res != LUSD_SUCCESS) {
        printf("[lightusd-wasm] Failed to create instance: %d\n", (int)res);
        return val::null();
    }

    // Load stage from memory
    LusdStageLoadFromMemoryInfo loadInfo = {};
    loadInfo.sType = LUSD_STRUCTURE_TYPE_STAGE_LOAD_FROM_MEMORY_INFO;
    loadInfo.pData = data.data();
    loadInfo.dataSize = length;

    // Determine format
    LusdFormat fmt = LUSD_FORMAT_USDC; // default
    if (format == 1) fmt = LUSD_FORMAT_USDA;
    else if (format == 2) fmt = LUSD_FORMAT_USDC;
    else if (format == 3) fmt = LUSD_FORMAT_USDZ;
    loadInfo.format = fmt;

    LusdStage stage = nullptr;
    res = lusdLoadStageFromMemory(instance, &loadInfo, &stage);
    if (res != LUSD_SUCCESS) {
        printf("[lightusd-wasm] Failed to load stage: %d\n", (int)res);
        lusdDestroyInstance(instance, nullptr);
        return val::null();
    }

    // Build the stage-level PBR material table (no layer required). Each mesh is then
    // matched to one of these via its material:binding relationship path.
    std::vector<StageMaterial> stageMaterials;
    {
        uint32_t matCount = 0;
        lusdStageGetMaterialCount(stage, &matCount);
        if (matCount > 0) {
            std::vector<LusdOpenPBRMaterial> mats(matCount);
            if (lusdStageGetMaterials(stage, matCount, mats.data()) == LUSD_SUCCESS) {
                for (uint32_t mi = 0; mi < matCount; ++mi) {
                    char pathBuf[512] = {0};
                    lusdStageGetMaterialPath(stage, mi, pathBuf, sizeof(pathBuf));
                    StageMaterial sm;
                    sm.path = pathBuf;
                    // OpenPBR base_color/metalness/roughness map onto our metallic-roughness viewer.
                    sm.baseColor[0] = mats[mi].base_color[0];
                    sm.baseColor[1] = mats[mi].base_color[1];
                    sm.baseColor[2] = mats[mi].base_color[2];
                    sm.metallic = mats[mi].base_metalness;
                    sm.roughness = mats[mi].base_roughness;
                    // Emission: luminance-weighted emission color.
                    sm.emissive[0] = mats[mi].emission_color[0] * mats[mi].emission_luminance;
                    sm.emissive[1] = mats[mi].emission_color[1] * mats[mi].emission_luminance;
                    sm.emissive[2] = mats[mi].emission_color[2] * mats[mi].emission_luminance;
                    stageMaterials.push_back(std::move(sm));
                }
            }
        }
        LUSD_LOG("[lightusd-wasm] %u stage material(s)\n", (unsigned)stageMaterials.size());
    }

    // Extract meshes
    std::vector<UsdMesh> meshes;
    uint32_t rootCount = 0;
    lusdStageGetRootPrimCount(stage, &rootCount);
    std::vector<LusdPrim> rootPrims(rootCount);
    lusdStageGetRootPrims(stage, rootCount, rootPrims.data());

    for (uint32_t i = 0; i < rootCount; ++i) {
        extractMeshesRecursive(rootPrims[i], meshes, stage, instance, stageMaterials);
    }

    LUSD_LOG("[lightusd-wasm] Extracted %u meshes\n", (unsigned)meshes.size());

    // Build JS result: array of { name, vertices (Float32Array), vertexCount, bbox }
    val result = val::array();
    for (size_t i = 0; i < meshes.size(); ++i) {
        const UsdMesh& m = meshes[i];
        val meshObj = val::object();
        meshObj.set("name", m.name);
        meshObj.set("vertexCount", m.vertexCount);

        // Create typed array view from WASM memory
        val vertData = val(typed_memory_view(m.vertices.size(), m.vertices.data()));
        // Copy to a new Float32Array (data will be freed after this function)
        val f32 = val::global("Float32Array").new_(m.vertices.size());
        f32.call<void>("set", vertData);
        meshObj.set("vertices", f32);

        val bbox = val::object();
        val bmin = val::array();
        bmin.call<void>("push", m.bboxMin[0]);
        bmin.call<void>("push", m.bboxMin[1]);
        bmin.call<void>("push", m.bboxMin[2]);
        val bmax = val::array();
        bmax.call<void>("push", m.bboxMax[0]);
        bmax.call<void>("push", m.bboxMax[1]);
        bmax.call<void>("push", m.bboxMax[2]);
        bbox.set("min", bmin);
        bbox.set("max", bmax);
        meshObj.set("bbox", bbox);

        // PBR material
        val material = val::object();
        material.set("hasMaterial", m.hasMaterial);
        val bc = val::array();
        bc.call<void>("push", m.baseColor[0]); bc.call<void>("push", m.baseColor[1]); bc.call<void>("push", m.baseColor[2]);
        material.set("baseColor", bc);
        material.set("metallic", m.metallic);
        material.set("roughness", m.roughness);
        val em = val::array();
        em.call<void>("push", m.emissive[0]); em.call<void>("push", m.emissive[1]); em.call<void>("push", m.emissive[2]);
        material.set("emissive", em);
        material.set("path", m.materialPath);
        meshObj.set("material", material);

        // Skinning (top-4 influences/vertex), parallel to vertices
        val skin = val::object();
        skin.set("hasSkin", m.hasSkin);
        skin.set("influences", m.influences);
        skin.set("skeletonPath", m.skeletonPath);
        if (m.hasSkin) {
            val jiView = val(typed_memory_view(m.jointIndices.size(), m.jointIndices.data()));
            val jiArr = val::global("Float32Array").new_(m.jointIndices.size());
            jiArr.call<void>("set", jiView);
            skin.set("jointIndices", jiArr);
            val jwView = val(typed_memory_view(m.jointWeights.size(), m.jointWeights.data()));
            val jwArr = val::global("Float32Array").new_(m.jointWeights.size());
            jwArr.call<void>("set", jwView);
            skin.set("jointWeights", jwArr);
        }
        meshObj.set("skin", skin);

        result.call<void>("push", meshObj);
    }

    lusdDestroyStage(instance, stage);
    lusdDestroyInstance(instance, nullptr);

    return result;
}

EMSCRIPTEN_BINDINGS(lightusd) {
    emscripten::function("loadUsdFromMemory", &loadUsdFromMemory);
}
