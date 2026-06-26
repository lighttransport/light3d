/*
 * gltf_binding.cc — glTF 2.0 path for the file-format-neutral WebGPU harness.
 *
 * Parses a .glb/.gltf from memory with tinygltf v3 (deps/tinygltf) and emits the
 * SAME neutral scene contract the viewer consumes for any source format:
 *
 *   loadGltfFromMemory(Uint8Array) -> {
 *     meshes: [{
 *       name, vertexCount,
 *       vertices : Float32Array (vc * 8)   // pos3 + normal3 + uv2, interleaved, indexed
 *       indices  : Uint32Array,
 *       bbox     : { min:[3], max:[3] },
 *       skin     : { hasSkin, skinIndex, jointIndices:Float32(vc*4), jointWeights:Float32(vc*4) },
 *       morph    : { count, weights:Float32(count), targets:[Float32(vc*3) position deltas ...] },
 *       material : { hasMaterial, baseColor:[4], metallic, roughness, emissive:[3] },
 *       nodeMatrix : Float32Array(16)      // world xform (identity for skinned meshes)
 *     }],
 *     skins: [{ jointCount, skinningMatrices:Float32(jc*16) }]  // jointWorld * inverseBind (bind pose)
 *   }
 *
 * Skinning/blendshape/PBR data that USD's lightusd-c build could not surface is fully
 * available here, so the neutral renderer lights up its skinning/morph/PBR paths.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <array>
#include <functional>

#include "wasm_log.h"
#include "tiny_gltf_v3.h"

namespace {

// ---------- accessor decoding ----------
float readComp(const uint8_t* p, int ct, int normalized) {
    switch (ct) {
        case TG3_COMPONENT_TYPE_FLOAT:          { float f; std::memcpy(&f, p, 4); return f; }
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:  { uint8_t  v = *p;                 return normalized ? v / 255.0f   : (float)v; }
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: { uint16_t v; std::memcpy(&v,p,2); return normalized ? v / 65535.0f  : (float)v; }
        case TG3_COMPONENT_TYPE_UNSIGNED_INT:   { uint32_t v; std::memcpy(&v,p,4); return (float)v; }
        case TG3_COMPONENT_TYPE_BYTE:           { int8_t  v = *(const int8_t*)p;   return normalized ? fmaxf(v / 127.0f,  -1.0f) : (float)v; }
        case TG3_COMPONENT_TYPE_SHORT:          { int16_t v; std::memcpy(&v,p,2);  return normalized ? fmaxf(v / 32767.0f,-1.0f) : (float)v; }
        default: return 0.0f;
    }
}

// Read an accessor as a flat float array (count * numComponents). Empty if invalid.
std::vector<float> readAccessor(const tg3_model* m, int accIdx) {
    std::vector<float> out;
    if (accIdx < 0 || (uint32_t)accIdx >= m->accessors_count) return out;
    const tg3_accessor* a = &m->accessors[accIdx];
    const int nc = tg3_num_components(a->type);
    const int cs = tg3_component_size(a->component_type);
    out.assign((size_t)a->count * nc, 0.0f);
    if (a->buffer_view < 0 || (uint32_t)a->buffer_view >= m->buffer_views_count) return out; // sparse-only: zeros
    const tg3_buffer_view* bv = &m->buffer_views[a->buffer_view];
    if (bv->buffer < 0 || (uint32_t)bv->buffer >= m->buffers_count) return out;
    const tg3_buffer* buf = &m->buffers[bv->buffer];
    int stride = tg3_accessor_byte_stride(a, bv);
    if (stride <= 0) stride = cs * nc;
    const uint8_t* base = buf->data.data + bv->byte_offset + a->byte_offset;
    for (uint64_t i = 0; i < a->count; ++i) {
        const uint8_t* e = base + i * stride;
        for (int c = 0; c < nc; ++c) out[i * nc + c] = readComp(e + c * cs, a->component_type, a->normalized);
    }
    return out;
}

std::vector<uint32_t> readIndices(const tg3_model* m, int accIdx, uint32_t vc) {
    std::vector<uint32_t> out;
    if (accIdx < 0 || (uint32_t)accIdx >= m->accessors_count) {
        out.resize(vc);
        for (uint32_t i = 0; i < vc; ++i) out[i] = i;
        return out;
    }
    const tg3_accessor* a = &m->accessors[accIdx];
    const tg3_buffer_view* bv = &m->buffer_views[a->buffer_view];
    const tg3_buffer* buf = &m->buffers[bv->buffer];
    const int cs = tg3_component_size(a->component_type);
    int stride = tg3_accessor_byte_stride(a, bv);
    if (stride <= 0) stride = cs;
    const uint8_t* base = buf->data.data + bv->byte_offset + a->byte_offset;
    out.resize(a->count);
    for (uint64_t i = 0; i < a->count; ++i) {
        const uint8_t* e = base + i * stride;
        uint32_t v = 0;
        if (a->component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE)  v = *e;
        else if (a->component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) { uint16_t t; std::memcpy(&t,e,2); v = t; }
        else { uint32_t t; std::memcpy(&t,e,4); v = t; }
        out[i] = v;
    }
    return out;
}

// ---------- column-major mat4 ----------
using Mat4 = std::array<float,16>;
Mat4 mat4Id() { Mat4 m{}; m[0]=m[5]=m[10]=m[15]=1; return m; }
Mat4 mat4Mul(const Mat4& a, const Mat4& b) { // a*b, column-major
    Mat4 o{};
    for (int c=0;c<4;++c) for (int r=0;r<4;++r) { float s=0; for (int k=0;k<4;++k) s+=a[k*4+r]*b[c*4+k]; o[c*4+r]=s; }
    return o;
}
Mat4 nodeLocal(const tg3_node* n) {
    Mat4 m{};
    if (n->has_matrix) { for (int i=0;i<16;++i) m[i]=(float)n->matrix[i]; return m; } // glTF matrix is column-major
    const double* t=n->translation; const double* q=n->rotation; const double* s=n->scale;
    const float x=(float)q[0],y=(float)q[1],z=(float)q[2],w=(float)q[3];
    const float xx=x*x,yy=y*y,zz=z*z,xy=x*y,xz=x*z,yz=y*z,wx=w*x,wy=w*y,wz=w*z;
    // column-major R*S then translation
    m[0]=(1-2*(yy+zz))*(float)s[0]; m[1]=(2*(xy+wz))*(float)s[0];   m[2]=(2*(xz-wy))*(float)s[0];   m[3]=0;
    m[4]=(2*(xy-wz))*(float)s[1];   m[5]=(1-2*(xx+zz))*(float)s[1]; m[6]=(2*(yz+wx))*(float)s[1];   m[7]=0;
    m[8]=(2*(xz+wy))*(float)s[2];   m[9]=(2*(yz-wx))*(float)s[2];   m[10]=(1-2*(xx+yy))*(float)s[2];m[11]=0;
    m[12]=(float)t[0]; m[13]=(float)t[1]; m[14]=(float)t[2]; m[15]=1;
    return m;
}

// Compute world matrix for every node (forest; memoized over parent links).
std::vector<Mat4> computeNodeWorld(const tg3_model* m) {
    const uint32_t N = m->nodes_count;
    std::vector<int> parent(N, -1);
    for (uint32_t i=0;i<N;++i)
        for (uint32_t c=0;c<m->nodes[i].children_count;++c) {
            int ch = m->nodes[i].children[c];
            if (ch>=0 && (uint32_t)ch<N) parent[ch]=(int)i;
        }
    std::vector<Mat4> world(N);
    std::vector<char> done(N, 0);
    std::function<void(int)> calc = [&](int i){
        if (done[i]) return;
        Mat4 local = nodeLocal(&m->nodes[i]);
        if (parent[i] >= 0) { calc(parent[i]); world[i] = mat4Mul(world[parent[i]], local); }
        else world[i] = local;
        done[i] = 1;
    };
    for (uint32_t i=0;i<N;++i) calc((int)i);
    return world;
}

// ---------- JS helpers ----------
emscripten::val f32(const std::vector<float>& v) {
    using namespace emscripten;
    val view = val(typed_memory_view(v.size(), v.data()));
    val arr = val::global("Float32Array").new_(v.size());
    arr.call<void>("set", view);
    return arr;
}
emscripten::val u32(const std::vector<uint32_t>& v) {
    using namespace emscripten;
    val view = val(typed_memory_view(v.size(), v.data()));
    val arr = val::global("Uint32Array").new_(v.size());
    arr.call<void>("set", view);
    return arr;
}
emscripten::val arr3(const float* v) {
    using namespace emscripten; val a = val::array();
    a.call<void>("push", v[0]); a.call<void>("push", v[1]); a.call<void>("push", v[2]); return a;
}
emscripten::val arr3d(const double* v) {
    using namespace emscripten; val a = val::array();
    a.call<void>("push", (float)v[0]); a.call<void>("push", (float)v[1]); a.call<void>("push", (float)v[2]); return a;
}
emscripten::val iarr(const std::vector<int32_t>& v) {
    using namespace emscripten;
    val view = val(typed_memory_view(v.size(), v.data()));
    val a = val::global("Int32Array").new_(v.size());
    a.call<void>("set", view);
    return a;
}

int findAttr(const tg3_primitive* p, const char* name) {
    for (uint32_t i=0;i<p->attributes_count;++i)
        if (tg3_str_equals_cstr(p->attributes[i].key, name)) return p->attributes[i].value;
    return -1;
}
int findTargetAttr(const tg3_str_int_pair* tgt, uint32_t cnt, const char* name) {
    for (uint32_t i=0;i<cnt;++i)
        if (tg3_str_equals_cstr(tgt[i].key, name)) return tgt[i].value;
    return -1;
}

} // namespace


emscripten::val loadGltfFromMemory(emscripten::val jsData) {
    using namespace emscripten;

    auto length = jsData["length"].as<unsigned>();
    std::vector<uint8_t> data(length);
    val mv = val(typed_memory_view(length, data.data()));
    mv.call<void>("set", jsData);

    tg3_model model; tg3_error_stack errors; tg3_parse_options opts;
    tg3_error_stack_init(&errors);
    tg3_parse_options_init(&opts);
    tg3_error_code err = tg3_parse_auto(&model, &errors, data.data(), length, "", 0, &opts);
    if (err != TG3_OK) {
        printf("[gltf-wasm] parse failed: %u bytes -> err=%d\n", length, (int)err);
        tg3_model_free(&model);
        tg3_error_stack_free(&errors);
        return val::null();
    }
    LUSD_LOG("[gltf-wasm] parse %u bytes -> meshes=%u skins=%u nodes=%u materials=%u\n",
             length, model.meshes_count, model.skins_count, model.nodes_count, model.materials_count);

    std::vector<Mat4> world = computeNodeWorld(&model);

    val result = val::object();

    // --- skins: precompute skinningMatrices = jointWorld * inverseBind (bind pose) ---
    val jsSkins = val::array();
    for (uint32_t s=0;s<model.skins_count;++s) {
        const tg3_skin* sk = &model.skins[s];
        const uint32_t jc = sk->joints_count;
        std::vector<float> ibm = readAccessor(&model, sk->inverse_bind_matrices); // jc*16 or empty
        std::vector<float> skinMats(jc*16, 0.0f);
        for (uint32_t j=0;j<jc;++j) {
            Mat4 inv = mat4Id();
            if (ibm.size() >= (size_t)(j+1)*16) for (int k=0;k<16;++k) inv[k]=ibm[j*16+k];
            int nodeIdx = sk->joints[j];
            Mat4 jw = (nodeIdx>=0 && (uint32_t)nodeIdx<world.size()) ? world[nodeIdx] : mat4Id();
            Mat4 sm = mat4Mul(jw, inv);
            for (int k=0;k<16;++k) skinMats[j*16+k]=sm[k];
        }
        val o = val::object();
        o.set("jointCount", jc);
        o.set("skinningMatrices", f32(skinMats));        // bind-pose default
        // Data needed to re-skin per animation frame on the JS side:
        std::vector<int32_t> jts(jc);
        for (uint32_t j=0;j<jc;++j) jts[j] = sk->joints[j];
        o.set("joints", iarr(jts));
        std::vector<float> ibmOut(jc*16, 0.0f);
        for (uint32_t j=0;j<jc;++j) {
            if (ibm.size() >= (size_t)(j+1)*16) for (int k=0;k<16;++k) ibmOut[j*16+k]=ibm[j*16+k];
            else { ibmOut[j*16+0]=ibmOut[j*16+5]=ibmOut[j*16+10]=ibmOut[j*16+15]=1.0f; }
        }
        o.set("inverseBindMatrices", f32(ibmOut));
        jsSkins.call<void>("push", o);
    }
    result.set("skins", jsSkins);

    // --- meshes: emit per (node-with-mesh, primitive) so world xform + skin bind correctly ---
    val jsMeshes = val::array();
    for (uint32_t ni=0; ni<model.nodes_count; ++ni) {
        const tg3_node* node = &model.nodes[ni];
        if (node->mesh < 0 || (uint32_t)node->mesh >= model.meshes_count) continue;
        const tg3_mesh* mesh = &model.meshes[node->mesh];
        const bool hasSkin = node->skin >= 0;

        for (uint32_t pi=0; pi<mesh->primitives_count; ++pi) {
            const tg3_primitive* prim = &mesh->primitives[pi];
            const int posAcc = findAttr(prim, "POSITION");
            if (posAcc < 0) continue;
            std::vector<float> pos = readAccessor(&model, posAcc);
            const uint32_t vc = (uint32_t)(pos.size()/3);
            if (vc == 0) continue;

            std::vector<float> nrm = readAccessor(&model, findAttr(prim, "NORMAL"));   // vc*3 or empty
            std::vector<float> uv  = readAccessor(&model, findAttr(prim, "TEXCOORD_0"));// vc*2 or empty

            // interleave pos3+normal3+uv2
            std::vector<float> verts(vc*8);
            float bmin[3]={1e30f,1e30f,1e30f}, bmax[3]={-1e30f,-1e30f,-1e30f};
            for (uint32_t v=0; v<vc; ++v) {
                float px=pos[v*3+0], py=pos[v*3+1], pz=pos[v*3+2];
                verts[v*8+0]=px; verts[v*8+1]=py; verts[v*8+2]=pz;
                verts[v*8+3]= nrm.size()>=(size_t)(v+1)*3 ? nrm[v*3+0]:0.0f;
                verts[v*8+4]= nrm.size()>=(size_t)(v+1)*3 ? nrm[v*3+1]:1.0f;
                verts[v*8+5]= nrm.size()>=(size_t)(v+1)*3 ? nrm[v*3+2]:0.0f;
                verts[v*8+6]= uv.size()>=(size_t)(v+1)*2 ? uv[v*2+0]:0.0f;
                verts[v*8+7]= uv.size()>=(size_t)(v+1)*2 ? uv[v*2+1]:0.0f;
                bmin[0]=fminf(bmin[0],px); bmin[1]=fminf(bmin[1],py); bmin[2]=fminf(bmin[2],pz);
                bmax[0]=fmaxf(bmax[0],px); bmax[1]=fmaxf(bmax[1],py); bmax[2]=fmaxf(bmax[2],pz);
            }
            std::vector<uint32_t> idx = readIndices(&model, prim->indices, vc);

            val mo = val::object();
            mo.set("name", std::string(mesh->name.data ? std::string(mesh->name.data, mesh->name.len) : "mesh"));
            mo.set("vertexCount", vc);
            mo.set("vertices", f32(verts));
            mo.set("indices", u32(idx));
            { val bb=val::object(); bb.set("min", arr3(bmin)); bb.set("max", arr3(bmax)); mo.set("bbox", bb); }

            // skinning
            val skin = val::object();
            skin.set("hasSkin", hasSkin);
            skin.set("skinIndex", node->skin);
            if (hasSkin) {
                std::vector<float> ji = readAccessor(&model, findAttr(prim, "JOINTS_0"));  // vc*4 (ints as float)
                std::vector<float> jw = readAccessor(&model, findAttr(prim, "WEIGHTS_0")); // vc*4
                if (ji.size() >= (size_t)vc*4 && jw.size() >= (size_t)vc*4) {
                    skin.set("jointIndices", f32(ji));
                    skin.set("jointWeights", f32(jw));
                } else {
                    skin.set("hasSkin", false);
                }
            }
            mo.set("skin", skin);

            // morph targets (POSITION deltas)
            val morph = val::object();
            uint32_t tcount = prim->targets_count;
            morph.set("count", tcount);
            if (tcount > 0) {
                val targets = val::array();
                for (uint32_t t=0;t<tcount;++t) {
                    int dpos = findTargetAttr(prim->targets[t], prim->target_attribute_counts[t], "POSITION");
                    std::vector<float> d = readAccessor(&model, dpos); // vc*3 or empty
                    if (d.size() < (size_t)vc*3) d.assign((size_t)vc*3, 0.0f);
                    targets.call<void>("push", f32(d));
                }
                morph.set("targets", targets);
                // initial weights: node.weights, else mesh.weights, else zeros
                std::vector<float> w(tcount, 0.0f);
                const double* ws=nullptr; uint32_t wn=0;
                if (node->weights_count==tcount) { ws=node->weights; wn=tcount; }
                else if (mesh->weights_count==tcount) { ws=mesh->weights; wn=tcount; }
                for (uint32_t k=0;k<wn;++k) w[k]=(float)ws[k];
                morph.set("weights", f32(w));
            }
            mo.set("morph", morph);

            // PBR material (metallic-roughness factors)
            val material = val::object();
            float baseColor[4]={0.8f,0.8f,0.8f,1.0f}, emissive[3]={0,0,0};
            float metallic=1.0f, roughness=1.0f; bool hasMat=false;
            if (prim->material >= 0 && (uint32_t)prim->material < model.materials_count) {
                const tg3_material* mt = &model.materials[prim->material];
                const tg3_pbr_metallic_roughness* pbr = &mt->pbr_metallic_roughness;
                for (int k=0;k<4;++k) baseColor[k]=(float)pbr->base_color_factor[k];
                metallic=(float)pbr->metallic_factor; roughness=(float)pbr->roughness_factor;
                for (int k=0;k<3;++k) emissive[k]=(float)mt->emissive_factor[k];
                hasMat=true;
            }
            material.set("hasMaterial", hasMat);
            { val bc=val::array(); for(int k=0;k<4;++k) bc.call<void>("push", baseColor[k]); material.set("baseColor", bc); }
            material.set("metallic", metallic);
            material.set("roughness", roughness);
            material.set("emissive", arr3(emissive));
            mo.set("material", material);

            // node world transform (identity for skinned meshes — joints carry the transform)
            Mat4 nm = (hasSkin || ni>=world.size()) ? mat4Id() : world[ni];
            mo.set("nodeMatrix", f32(std::vector<float>(nm.begin(), nm.end())));

            jsMeshes.call<void>("push", mo);
        }
    }
    result.set("meshes", jsMeshes);

    // --- nodes (TRS hierarchy) so JS can recompute joint world transforms per anim frame ---
    val jsNodes = val::array();
    for (uint32_t i=0;i<model.nodes_count;++i) {
        const tg3_node* n = &model.nodes[i];
        val o = val::object();
        o.set("t", arr3d(n->translation));
        { val r=val::array(); for(int k=0;k<4;++k) r.call<void>("push",(float)n->rotation[k]); o.set("r", r); }
        o.set("s", arr3d(n->scale));
        o.set("hasMatrix", n->has_matrix ? true : false);
        if (n->has_matrix) { val m=val::array(); for(int k=0;k<16;++k) m.call<void>("push",(float)n->matrix[k]); o.set("matrix", m); }
        { val ch=val::array(); for(uint32_t c=0;c<n->children_count;++c) ch.call<void>("push",(int)n->children[c]); o.set("children", ch); }
        jsNodes.call<void>("push", o);
    }
    result.set("nodes", jsNodes);

    // --- animations (sampler curves per target node/path) ---
    val jsAnims = val::array();
    for (uint32_t a=0;a<model.animations_count;++a) {
        const tg3_animation* an = &model.animations[a];
        float duration = 0.0f;
        val channels = val::array();
        for (uint32_t c=0;c<an->channels_count;++c) {
            const tg3_animation_channel* ch = &an->channels[c];
            if (ch->sampler < 0 || (uint32_t)ch->sampler >= an->samplers_count) continue;
            const tg3_animation_sampler* sp = &an->samplers[ch->sampler];
            std::vector<float> times = readAccessor(&model, sp->input);
            std::vector<float> values = readAccessor(&model, sp->output);
            if (!times.empty()) duration = fmaxf(duration, times[times.size()-1]);
            val co = val::object();
            co.set("node", ch->target.node);
            co.set("path", std::string(ch->target.path.data ? std::string(ch->target.path.data, ch->target.path.len) : ""));
            co.set("interp", std::string(sp->interpolation.data ? std::string(sp->interpolation.data, sp->interpolation.len) : "LINEAR"));
            co.set("times", f32(times));
            co.set("values", f32(values));
            channels.call<void>("push", co);
        }
        val ao = val::object();
        ao.set("name", std::string(an->name.data ? std::string(an->name.data, an->name.len) : "anim"));
        ao.set("duration", duration);
        ao.set("channels", channels);
        jsAnims.call<void>("push", ao);
    }
    result.set("animations", jsAnims);

    tg3_model_free(&model);
    tg3_error_stack_free(&errors);
    return result;
}

EMSCRIPTEN_BINDINGS(lightusd_gltf) {
    emscripten::function("loadGltfFromMemory", &loadGltfFromMemory);
}
