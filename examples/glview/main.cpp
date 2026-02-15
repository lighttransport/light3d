#ifdef GLVIEW_OPENGL_ENABLED
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h>

#ifdef GLVIEW_VULKAN_ENABLED
#include "vk_backend.h"
#endif

#include <light3d/light3d.h>
#include <light3d/obj_loader.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Inline math utilities
// ---------------------------------------------------------------------------
struct Vec3 {
    float x, y, z;
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { float inv = 1.0f / s; return {x * inv, y * inv, z * inv}; }
};

static float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

static float vec3Length(Vec3 v) { return std::sqrt(dot(v, v)); }

static Vec3 normalize(Vec3 v) {
    float len = vec3Length(v);
    if (len < 1e-8f) return {0, 0, 0};
    return v * (1.0f / len);
}

static Vec3 vec3Min(Vec3 a, Vec3 b) {
    return {std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z)};
}

static Vec3 vec3Max(Vec3 a, Vec3 b) {
    return {std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z)};
}

// 4x4 column-major matrix
struct Mat4 {
    float m[16] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    // Column-major access: m[col*4 + row]
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += at(row, k) * b.at(k, c);
                r.at(row, c) = sum;
            }
        return r;
    }

    static Mat4 translate(Vec3 t) {
        Mat4 r = identity();
        r.at(0, 3) = t.x;
        r.at(1, 3) = t.y;
        r.at(2, 3) = t.z;
        return r;
    }

    static Mat4 scale(Vec3 s) {
        Mat4 r;
        r.at(0, 0) = s.x;
        r.at(1, 1) = s.y;
        r.at(2, 2) = s.z;
        r.at(3, 3) = 1.0f;
        return r;
    }

    Mat4 inverse() const {
        // Cofactor expansion for general 4x4
        float inv[16];
        const float* mm = m;

        inv[0] = mm[5]*mm[10]*mm[15] - mm[5]*mm[11]*mm[14] -
                 mm[9]*mm[6]*mm[15]  + mm[9]*mm[7]*mm[14]  +
                 mm[13]*mm[6]*mm[11] - mm[13]*mm[7]*mm[10];
        inv[4] = -mm[4]*mm[10]*mm[15] + mm[4]*mm[11]*mm[14] +
                  mm[8]*mm[6]*mm[15]  - mm[8]*mm[7]*mm[14]  -
                  mm[12]*mm[6]*mm[11] + mm[12]*mm[7]*mm[10];
        inv[8] = mm[4]*mm[9]*mm[15] - mm[4]*mm[11]*mm[13] -
                 mm[8]*mm[5]*mm[15] + mm[8]*mm[7]*mm[13]  +
                 mm[12]*mm[5]*mm[11] - mm[12]*mm[7]*mm[9];
        inv[12] = -mm[4]*mm[9]*mm[14] + mm[4]*mm[10]*mm[13] +
                   mm[8]*mm[5]*mm[14] - mm[8]*mm[6]*mm[13]  -
                   mm[12]*mm[5]*mm[10] + mm[12]*mm[6]*mm[9];

        inv[1] = -mm[1]*mm[10]*mm[15] + mm[1]*mm[11]*mm[14] +
                  mm[9]*mm[2]*mm[15]  - mm[9]*mm[3]*mm[14]  -
                  mm[13]*mm[2]*mm[11] + mm[13]*mm[3]*mm[10];
        inv[5] = mm[0]*mm[10]*mm[15] - mm[0]*mm[11]*mm[14] -
                 mm[8]*mm[2]*mm[15]  + mm[8]*mm[3]*mm[14]  +
                 mm[12]*mm[2]*mm[11] - mm[12]*mm[3]*mm[10];
        inv[9] = -mm[0]*mm[9]*mm[15] + mm[0]*mm[11]*mm[13] +
                  mm[8]*mm[1]*mm[15] - mm[8]*mm[3]*mm[13]  -
                  mm[12]*mm[1]*mm[11] + mm[12]*mm[3]*mm[9];
        inv[13] = mm[0]*mm[9]*mm[14] - mm[0]*mm[10]*mm[13] -
                  mm[8]*mm[1]*mm[14] + mm[8]*mm[2]*mm[13]  +
                  mm[12]*mm[1]*mm[10] - mm[12]*mm[2]*mm[9];

        inv[2] = mm[1]*mm[6]*mm[15] - mm[1]*mm[7]*mm[14] -
                 mm[5]*mm[2]*mm[15] + mm[5]*mm[3]*mm[14]  +
                 mm[13]*mm[2]*mm[7]  - mm[13]*mm[3]*mm[6];
        inv[6] = -mm[0]*mm[6]*mm[15] + mm[0]*mm[7]*mm[14] +
                  mm[4]*mm[2]*mm[15] - mm[4]*mm[3]*mm[14]  -
                  mm[12]*mm[2]*mm[7]  + mm[12]*mm[3]*mm[6];
        inv[10] = mm[0]*mm[5]*mm[15] - mm[0]*mm[7]*mm[13] -
                  mm[4]*mm[1]*mm[15] + mm[4]*mm[3]*mm[13]  +
                  mm[12]*mm[1]*mm[7]  - mm[12]*mm[3]*mm[5];
        inv[14] = -mm[0]*mm[5]*mm[14] + mm[0]*mm[6]*mm[13] +
                   mm[4]*mm[1]*mm[14] - mm[4]*mm[2]*mm[13]  -
                   mm[12]*mm[1]*mm[6]  + mm[12]*mm[2]*mm[5];

        inv[3] = -mm[1]*mm[6]*mm[11] + mm[1]*mm[7]*mm[10] +
                  mm[5]*mm[2]*mm[11] - mm[5]*mm[3]*mm[10]  -
                  mm[9]*mm[2]*mm[7]   + mm[9]*mm[3]*mm[6];
        inv[7] = mm[0]*mm[6]*mm[11] - mm[0]*mm[7]*mm[10] -
                 mm[4]*mm[2]*mm[11] + mm[4]*mm[3]*mm[10]  +
                 mm[8]*mm[2]*mm[7]   - mm[8]*mm[3]*mm[6];
        inv[11] = -mm[0]*mm[5]*mm[11] + mm[0]*mm[7]*mm[9]  +
                   mm[4]*mm[1]*mm[11] - mm[4]*mm[3]*mm[9]   -
                   mm[8]*mm[1]*mm[7]   + mm[8]*mm[3]*mm[5];
        inv[15] = mm[0]*mm[5]*mm[10] - mm[0]*mm[6]*mm[9]  -
                  mm[4]*mm[1]*mm[10] + mm[4]*mm[2]*mm[9]   +
                  mm[8]*mm[1]*mm[6]   - mm[8]*mm[2]*mm[5];

        float det = mm[0]*inv[0] + mm[1]*inv[4] + mm[2]*inv[8] + mm[3]*inv[12];
        if (std::fabs(det) < 1e-12f) return identity();

        Mat4 result;
        float invDet = 1.0f / det;
        for (int i = 0; i < 16; ++i) result.m[i] = inv[i] * invDet;
        return result;
    }
};

static Vec3 mat4TransformPoint(const Mat4& m, Vec3 p) {
    float x = m.at(0,0)*p.x + m.at(0,1)*p.y + m.at(0,2)*p.z + m.at(0,3);
    float y = m.at(1,0)*p.x + m.at(1,1)*p.y + m.at(1,2)*p.z + m.at(1,3);
    float z = m.at(2,0)*p.x + m.at(2,1)*p.y + m.at(2,2)*p.z + m.at(2,3);
    float w = m.at(3,0)*p.x + m.at(3,1)*p.y + m.at(3,2)*p.z + m.at(3,3);
    if (std::fabs(w) > 1e-8f) { x /= w; y /= w; z /= w; }
    return {x, y, z};
}

static void extractNormalMatrix(const Mat4& model, float out[9]) {
    // Upper-left 3x3 of model matrix (for uniform-scale this is sufficient)
    out[0] = model.at(0,0); out[1] = model.at(1,0); out[2] = model.at(2,0);
    out[3] = model.at(0,1); out[4] = model.at(1,1); out[5] = model.at(2,1);
    out[6] = model.at(0,2); out[7] = model.at(1,2); out[8] = model.at(2,2);
}

static Mat4 perspective(float fovDeg, float aspect, float near, float far) {
    float fovRad = fovDeg * (3.14159265f / 180.0f);
    float tanHalf = std::tan(fovRad * 0.5f);
    Mat4 r;
    r.at(0, 0) = 1.0f / (aspect * tanHalf);
    r.at(1, 1) = 1.0f / tanHalf;
    r.at(2, 2) = -(far + near) / (far - near);
    r.at(3, 2) = -1.0f;
    r.at(2, 3) = -(2.0f * far * near) / (far - near);
    return r;
}

// Vulkan clip space: Y-down, Z=[0,1]
static Mat4 perspectiveVk(float fovDeg, float aspect, float nearP, float farP) {
    float fovRad = fovDeg * (3.14159265f / 180.0f);
    float tanHalf = std::tan(fovRad * 0.5f);
    Mat4 r;
    r.at(0, 0) = 1.0f / (aspect * tanHalf);
    r.at(1, 1) = -1.0f / tanHalf;  // negate Y for Vulkan
    r.at(2, 2) = farP / (nearP - farP);  // Z remap to [0,1]
    r.at(3, 2) = -1.0f;
    r.at(2, 3) = -(farP * nearP) / (farP - nearP);
    return r;
}

static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 f = normalize(target - eye);
    Vec3 r = normalize(cross(f, up));
    Vec3 u = cross(r, f);

    Mat4 m = Mat4::identity();
    m.at(0, 0) = r.x;   m.at(0, 1) = r.y;   m.at(0, 2) = r.z;
    m.at(1, 0) = u.x;   m.at(1, 1) = u.y;   m.at(1, 2) = u.z;
    m.at(2, 0) = -f.x;  m.at(2, 1) = -f.y;  m.at(2, 2) = -f.z;
    m.at(0, 3) = -dot(r, eye);
    m.at(1, 3) = -dot(u, eye);
    m.at(2, 3) = dot(f, eye);
    return m;
}

// ---------------------------------------------------------------------------
// BBox
// ---------------------------------------------------------------------------
struct BBox {
    Vec3 mn = { 1e30f,  1e30f,  1e30f};
    Vec3 mx = {-1e30f, -1e30f, -1e30f};

    void expand(Vec3 p) {
        mn = vec3Min(mn, p);
        mx = vec3Max(mx, p);
    }

    Vec3 center() const { return (mn + mx) * 0.5f; }
    Vec3 size() const { return mx - mn; }

    float radius() const {
        Vec3 s = size();
        return vec3Length(s) * 0.5f;
    }

    BBox transformed(Vec3 pos, Vec3 scl) const {
        // For axis-aligned scale + translate
        Vec3 a = {mn.x * scl.x + pos.x, mn.y * scl.y + pos.y, mn.z * scl.z + pos.z};
        Vec3 b = {mx.x * scl.x + pos.x, mx.y * scl.y + pos.y, mx.z * scl.z + pos.z};
        return {vec3Min(a, b), vec3Max(a, b)};
    }
};

// ---------------------------------------------------------------------------
// Ray
// ---------------------------------------------------------------------------
struct Ray {
    Vec3 origin, direction;
};

// ---------------------------------------------------------------------------
// OrbitCamera (Maya-style)
// ---------------------------------------------------------------------------
struct OrbitCamera {
    float longitude = 45.0f;
    float latitude  = 30.0f;
    float distance  = 8.0f;
    Vec3  target    = {0, 0, 0};

    Vec3 getEyePosition() const {
        float lonRad = longitude * (3.14159265f / 180.0f);
        float latRad = latitude * (3.14159265f / 180.0f);
        float cosLat = std::cos(latRad);
        return {
            target.x + distance * cosLat * std::sin(lonRad),
            target.y + distance * std::sin(latRad),
            target.z + distance * cosLat * std::cos(lonRad)
        };
    }

    Mat4 getViewMatrix() const {
        return lookAt(getEyePosition(), target, {0, 1, 0});
    }

    void getLocalAxes(Vec3& right, Vec3& up) const {
        Vec3 eye = getEyePosition();
        Vec3 f = normalize(target - eye);
        right = normalize(cross(f, {0, 1, 0}));
        up = cross(right, f);
    }
};

// ---------------------------------------------------------------------------
// Mesh (OpenGL)
// ---------------------------------------------------------------------------
#ifdef GLVIEW_OPENGL_ENABLED
struct Mesh {
    GLuint vao = 0, vbo = 0;
    int vertexCount = 0;
};
#endif

// ---------------------------------------------------------------------------
// SceneObject
// ---------------------------------------------------------------------------
struct SceneObject {
    std::string name;
#ifdef GLVIEW_OPENGL_ENABLED
    Mesh* mesh = nullptr;  // shared, not owned
#endif
    int meshIndex = 0;  // index into mesh array (for Vulkan path)
    Vec3 position = {0, 0, 0};
    Vec3 scl = {1, 1, 1};
    BBox localBBox;
    bool selected = false;
    int pickId = 0;  // 1-based

    BBox worldBBox() const {
        return localBBox.transformed(position, scl);
    }

    Mat4 modelMatrix() const {
        return Mat4::translate(position) * Mat4::scale(scl);
    }
};

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static OrbitCamera gCamera;
static float gAspect = 800.0f / 600.0f;
static double gLastCursorX = 0, gLastCursorY = 0;
static bool gLmbDown = false, gMmbDown = false, gRmbDown = false;
static bool gModCtrl = false, gModShift = false, gModAlt = false;

// Click detection
static double gClickPressX = 0, gClickPressY = 0;
static bool gPendingClick = false;
static double gPendingClickX = 0, gPendingClickY = 0;

// Render mode: 4=wireframe, 5=shading(default), 6=shading+texture, 7=lighting
static int gRenderMode = 5;

// Scene objects
static std::vector<SceneObject> gObjects;

// Backend selection
static bool gUseVulkan = false;

// OBJ file to load (empty = demo scene)
static std::string gObjFilePath;

#ifdef GLVIEW_OPENGL_ENABLED
// Pick FBO
static GLuint gPickFBO = 0;
static GLuint gPickColorTex = 0;
static GLuint gPickDepthRB = 0;
static int gPickFBOWidth = 0, gPickFBOHeight = 0;

// Checkerboard texture
static GLuint gCheckerTex = 0;
#endif

#ifdef GLVIEW_VULKAN_ENABLED
static VulkanBackend* gVkBackend = nullptr;
#endif

#ifdef GLVIEW_OPENGL_ENABLED
// ---------------------------------------------------------------------------
// Shader sources (OpenGL)
// ---------------------------------------------------------------------------

// --- Color shader (modes 4 & 5, grid, axis, selection box) ---
static const char* kColorVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aUV;

out vec3 vColor;

uniform mat4 uModel;
uniform mat4 uVP;

void main() {
    gl_Position = uVP * uModel * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* kColorFS = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

// --- Textured shader (mode 6) ---
static const char* kTexturedVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aUV;

out vec3 vColor;
out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uVP;

void main() {
    gl_Position = uVP * uModel * vec4(aPos, 1.0);
    vColor = aColor;
    vUV = aUV;
}
)";

static const char* kTexturedFS = R"(
#version 330 core
in vec3 vColor;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec3 texColor = texture(uTexture, vUV).rgb;
    FragColor = vec4(vColor * texColor, 1.0);
}
)";

// --- Lit shader (mode 7, Blinn-Phong) ---
static const char* kLitVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aUV;

out vec3 vColor;
out vec3 vNormal;
out vec3 vWorldPos;

uniform mat4 uModel;
uniform mat4 uVP;
uniform mat3 uNormalMatrix;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uVP * worldPos;
    vColor = aColor;
    vNormal = uNormalMatrix * aNormal;
    vWorldPos = worldPos.xyz;
}
)";

static const char* kLitFS = R"(
#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform vec3 uLightColor;
uniform float uAmbient;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 color = vColor * (uAmbient + diff * uLightColor) + spec * uLightColor * 0.3;
    FragColor = vec4(color, 1.0);
}
)";

// --- Pick shader (color-pick offscreen pass) ---
static const char* kPickVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uVP;

void main() {
    gl_Position = uVP * uModel * vec4(aPos, 1.0);
}
)";

static const char* kPickFS = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uPickColor;

void main() {
    FragColor = vec4(uPickColor, 1.0);
}
)";

// --- TUI overlay shader ---
static const char* kTuiVS = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aFgColor;
layout (location = 3) in vec3 aBgColor;
layout (location = 4) in float aBgAlpha;

out vec2 vUV;
out vec3 vFgColor;
out vec3 vBgColor;
out float vBgAlpha;

uniform vec2 uScreenSize;

void main() {
    vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;  // flip Y: pixel row 0 = top
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vFgColor = aFgColor;
    vBgColor = aBgColor;
    vBgAlpha = aBgAlpha;
}
)";

static const char* kTuiFS = R"(
#version 330 core
in vec2 vUV;
in vec3 vFgColor;
in vec3 vBgColor;
in float vBgAlpha;

out vec4 FragColor;

uniform sampler2D uFontAtlas;

void main() {
    float glyph = texture(uFontAtlas, vUV).r;
    vec3 color = mix(vBgColor, vFgColor, glyph);
    float alpha = mix(vBgAlpha, 1.0, glyph);
    FragColor = vec4(color, alpha);
}
)";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
#endif // GLVIEW_OPENGL_ENABLED

// ---------------------------------------------------------------------------
// Vertex format: pos(3) + color(3) + normal(3) + uv(2) = 11 floats
// ---------------------------------------------------------------------------
static const int kVertStride = 11;

static void pushVertex(std::vector<float>& buf,
                        float x, float y, float z,
                        float r, float g, float b,
                        float nx, float ny, float nz,
                        float u, float v) {
    buf.push_back(x);  buf.push_back(y);  buf.push_back(z);
    buf.push_back(r);  buf.push_back(g);  buf.push_back(b);
    buf.push_back(nx); buf.push_back(ny); buf.push_back(nz);
    buf.push_back(u);  buf.push_back(v);
}

// Convenience for grid/axis: normal=(0,1,0), uv=(0,0)
static void pushVertexSimple(std::vector<float>& buf,
                              float x, float y, float z,
                              float r, float g, float b) {
    pushVertex(buf, x, y, z, r, g, b, 0, 1, 0, 0, 0);
}

#ifdef GLVIEW_OPENGL_ENABLED
static Mesh createMesh(const std::vector<float>& verts) {
    Mesh m;
    m.vertexCount = static_cast<int>(verts.size()) / kVertStride;

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    GLsizei stride = kVertStride * sizeof(float);

    // location 0: position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);

    // location 1: color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // location 2: normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // location 3: uv
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(9 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    return m;
}

static void destroyMesh(Mesh& m) {
    glDeleteVertexArrays(1, &m.vao);
    glDeleteBuffers(1, &m.vbo);
    m.vao = m.vbo = 0;
    m.vertexCount = 0;
}
#endif // GLVIEW_OPENGL_ENABLED

// ---------------------------------------------------------------------------
// Geometry builders (return vertex data; upload happens separately)
// ---------------------------------------------------------------------------
static std::vector<float> buildGridVerts(int halfExtent, float step) {
    std::vector<float> verts;
    float gray = 0.4f;
    float limit = static_cast<float>(halfExtent) * step;
    for (int i = -halfExtent; i <= halfExtent; ++i) {
        float pos = static_cast<float>(i) * step;
        pushVertexSimple(verts, pos, 0, -limit, gray, gray, gray);
        pushVertexSimple(verts, pos, 0,  limit, gray, gray, gray);
        pushVertexSimple(verts, -limit, 0, pos, gray, gray, gray);
        pushVertexSimple(verts,  limit, 0, pos, gray, gray, gray);
    }
    return verts;
}

static std::vector<float> buildAxisVerts(float length) {
    std::vector<float> verts;
    pushVertexSimple(verts, 0, 0, 0, 1, 0, 0);
    pushVertexSimple(verts, length, 0, 0, 1, 0, 0);
    pushVertexSimple(verts, 0, 0, 0, 0, 1, 0);
    pushVertexSimple(verts, 0, length, 0, 0, 1, 0);
    pushVertexSimple(verts, 0, 0, 0, 0, 0, 1);
    pushVertexSimple(verts, 0, 0, length, 0, 0, 1);
    return verts;
}

static std::vector<float> buildCubeVerts() {
    std::vector<float> verts;

    struct Face {
        float v[4][3]; // 4 corners
        float r, g, b;
    };
    // clang-format off
    Face faces[] = {
        // +X
        {{{ 0.5f,-0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f}}, 0.9f, 0.4f, 0.4f},
        // -X
        {{{-0.5f,-0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f, 0.5f,-0.5f},{-0.5f,-0.5f,-0.5f}}, 0.5f, 0.2f, 0.2f},
        // +Y
        {{{-0.5f, 0.5f,-0.5f},{-0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f,-0.5f}}, 0.4f, 0.9f, 0.4f},
        // -Y
        {{{-0.5f,-0.5f, 0.5f},{-0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f, 0.5f}}, 0.2f, 0.5f, 0.2f},
        // +Z
        {{{-0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f}}, 0.4f, 0.4f, 0.9f},
        // -Z
        {{{ 0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f}}, 0.2f, 0.2f, 0.5f},
    };
    // clang-format on

    // Per-face UVs: 0=(0,0), 1=(1,0), 2=(1,1), 3=(0,1)
    float uvs[4][2] = {{0,0},{1,0},{1,1},{0,1}};

    for (auto& f : faces) {
        // Compute face normal from cross product of two edges
        Vec3 v0 = {f.v[0][0], f.v[0][1], f.v[0][2]};
        Vec3 v1 = {f.v[1][0], f.v[1][1], f.v[1][2]};
        Vec3 v2 = {f.v[2][0], f.v[2][1], f.v[2][2]};
        Vec3 e1 = v1 - v0;
        Vec3 e2 = v2 - v0;
        Vec3 n = normalize(cross(e1, e2));

        // Triangle 1: 0-1-2
        pushVertex(verts, f.v[0][0], f.v[0][1], f.v[0][2], f.r, f.g, f.b, n.x, n.y, n.z, uvs[0][0], uvs[0][1]);
        pushVertex(verts, f.v[1][0], f.v[1][1], f.v[1][2], f.r, f.g, f.b, n.x, n.y, n.z, uvs[1][0], uvs[1][1]);
        pushVertex(verts, f.v[2][0], f.v[2][1], f.v[2][2], f.r, f.g, f.b, n.x, n.y, n.z, uvs[2][0], uvs[2][1]);
        // Triangle 2: 0-2-3
        pushVertex(verts, f.v[0][0], f.v[0][1], f.v[0][2], f.r, f.g, f.b, n.x, n.y, n.z, uvs[0][0], uvs[0][1]);
        pushVertex(verts, f.v[2][0], f.v[2][1], f.v[2][2], f.r, f.g, f.b, n.x, n.y, n.z, uvs[2][0], uvs[2][1]);
        pushVertex(verts, f.v[3][0], f.v[3][1], f.v[3][2], f.r, f.g, f.b, n.x, n.y, n.z, uvs[3][0], uvs[3][1]);
    }

    return verts;
}

// Unit wireframe cube for selection highlight (-0.5 to +0.5), 12 edges, 24 verts
static std::vector<float> buildBBoxWireframeVerts() {
    std::vector<float> verts;
    float y = 1.0f; // yellow

    // 8 corners of unit cube
    float c[8][3] = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };
    // 12 edges (index pairs)
    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},  // back face
        {4,5},{5,6},{6,7},{7,4},  // front face
        {0,4},{1,5},{2,6},{3,7},  // connecting
    };
    for (auto& e : edges) {
        pushVertexSimple(verts, c[e[0]][0], c[e[0]][1], c[e[0]][2], y, y, 0);
        pushVertexSimple(verts, c[e[1]][0], c[e[1]][1], c[e[1]][2], y, y, 0);
    }
    return verts;
}

// ---------------------------------------------------------------------------
// Convert ObjLoadResult to interleaved 11-float triangulated vertex buffer
// ---------------------------------------------------------------------------
struct ObjConvertResult {
    std::vector<float> verts;
    BBox bbox;
};

static ObjConvertResult buildVertsFromObjResult(const light3d::ObjLoadResult& obj) {
    ObjConvertResult result;
    const auto& geo = obj.geometry;

    int faceStart = 0;
    for (size_t fi = 0; fi < geo.faceVertexCounts.size(); ++fi) {
        int fvc = geo.faceVertexCounts[fi];

        // Look up per-face material color
        float cr = 0.8f, cg = 0.8f, cb = 0.8f;
        if (fi < geo.faceMaterialIds.size()) {
            const light3d::Material* mat = obj.materials.getMaterial(geo.faceMaterialIds[fi]);
            if (mat) {
                cr = mat->baseColor.x;
                cg = mat->baseColor.y;
                cb = mat->baseColor.z;
            }
        }

        // Fan-triangulate: vertex 0 is the hub
        for (int t = 1; t + 1 < fvc; ++t) {
            int idx[3] = {
                geo.faceVertexIndices[faceStart],
                geo.faceVertexIndices[faceStart + t],
                geo.faceVertexIndices[faceStart + t + 1]
            };

            // Positions
            light3d::Vec3 p[3];
            for (int k = 0; k < 3; ++k) {
                p[k] = (idx[k] >= 0 && idx[k] < static_cast<int>(geo.points.size()))
                    ? geo.points[idx[k]] : light3d::Vec3(0, 0, 0);
            }

            // Normals
            light3d::Vec3 n[3];
            for (int k = 0; k < 3; ++k) {
                n[k] = (idx[k] >= 0 && idx[k] < static_cast<int>(geo.normals.size()))
                    ? geo.normals[idx[k]] : light3d::Vec3(0, 1, 0);
            }

            // UVs
            light3d::Vec3 uv[3];
            for (int k = 0; k < 3; ++k) {
                uv[k] = (idx[k] >= 0 && idx[k] < static_cast<int>(geo.uvs.size()))
                    ? geo.uvs[idx[k]] : light3d::Vec3(0, 0, 0);
            }

            for (int k = 0; k < 3; ++k) {
                pushVertex(result.verts,
                           p[k].x, p[k].y, p[k].z,
                           cr, cg, cb,
                           n[k].x, n[k].y, n[k].z,
                           uv[k].x, uv[k].y);
                result.bbox.expand({p[k].x, p[k].y, p[k].z});
            }
        }
        faceStart += fvc;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Checkerboard texture (64x64, 8px cells) [OpenGL only]
// ---------------------------------------------------------------------------
#ifdef GLVIEW_OPENGL_ENABLED
static GLuint createCheckerTexture() {
    const int sz = 64;
    const int cell = 8;
    unsigned char data[sz * sz * 3];
    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            bool white = ((x / cell) + (y / cell)) % 2 == 0;
            unsigned char val = white ? 255 : 180;
            int idx = (y * sz + x) * 3;
            data[idx] = data[idx + 1] = data[idx + 2] = val;
        }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sz, sz, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}
#endif // GLVIEW_OPENGL_ENABLED

// ---------------------------------------------------------------------------
// Embedded 8x16 bitmap font (CP437 style, ASCII 32-126 + box-drawing)
// ---------------------------------------------------------------------------
// Each glyph is 8 pixels wide, 16 pixels tall, stored as 16 bytes (1 bit/pixel).
// Glyphs 0-94 = ASCII 32 (' ') through 126 ('~')
// Glyphs 95-108 = box-drawing: ─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ █ ░
static const int kFontGlyphW = 8;
static const int kFontGlyphH = 16;
static const int kFontGlyphCount = 109; // 95 ASCII + 14 box-drawing

// Box-drawing character indices (offset from glyph 95)
static const int kBoxH     = 95;  // ─  horizontal line
static const int kBoxV     = 96;  // │  vertical line
static const int kBoxTL    = 97;  // ┌  top-left corner
static const int kBoxTR    = 98;  // ┐  top-right corner
static const int kBoxBL    = 99;  // └  bottom-left corner
static const int kBoxBR    = 100; // ┘  bottom-right corner
static const int kBoxLT    = 101; // ├  left tee
static const int kBoxRT    = 102; // ┤  right tee
static const int kBoxTT    = 103; // ┬  top tee
static const int kBoxBT    = 104; // ┴  bottom tee
static const int kBoxCross = 105; // ┼  cross
static const int kBoxFull  = 106; // █  full block
static const int kBoxShade = 107; // ░  light shade
static const int kBoxTitle = 108; // ─  same as H but used for title separator

static const uint8_t kFontData[kFontGlyphCount][16] = {
    // ASCII 32: ' ' (space)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 33: '!'
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    // ASCII 34: '"'
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 35: '#'
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00},
    // ASCII 36: '$'
    {0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x06,0x86,0xC6,0x7C,0x18,0x18,0x00,0x00},
    // ASCII 37: '%'
    {0x00,0x00,0x00,0x00,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0x00,0x00,0x00,0x00},
    // ASCII 38: '&'
    {0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // ASCII 39: '\''
    {0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 40: '('
    {0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00},
    // ASCII 41: ')'
    {0x00,0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00,0x00},
    // ASCII 42: '*'
    {0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 43: '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 44: ','
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x30,0x00,0x00,0x00},
    // ASCII 45: '-'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 46: '.'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    // ASCII 47: '/'
    {0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00},
    // ASCII 48: '0'
    {0x00,0x00,0x7C,0xC6,0xC6,0xCE,0xDE,0xF6,0xE6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 49: '1'
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},
    // ASCII 50: '2'
    {0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0x00,0x00,0x00,0x00},
    // ASCII 51: '3'
    {0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 52: '4'
    {0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00,0x00},
    // ASCII 53: '5'
    {0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 54: '6'
    {0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 55: '7'
    {0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},
    // ASCII 56: '8'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 57: '9'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},
    // ASCII 58: ':'
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    // ASCII 59: ';'
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00},
    // ASCII 60: '<'
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},
    // ASCII 61: '='
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 62: '>'
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},
    // ASCII 63: '?'
    {0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    // ASCII 64: '@'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 65: 'A'
    {0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 66: 'B'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,0x00},
    // ASCII 67: 'C'
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 68: 'D'
    {0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,0x00},
    // ASCII 69: 'E'
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    // ASCII 70: 'F'
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // ASCII 71: 'G'
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0x00,0x00,0x00,0x00},
    // ASCII 72: 'H'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 73: 'I'
    {0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 74: 'J'
    {0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00},
    // ASCII 75: 'K'
    {0x00,0x00,0xE6,0x66,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    // ASCII 76: 'L'
    {0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    // ASCII 77: 'M'
    {0x00,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 78: 'N'
    {0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 79: 'O'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 80: 'P'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // ASCII 81: 'Q'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0x00,0x00},
    // ASCII 82: 'R'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    // ASCII 83: 'S'
    {0x00,0x00,0x7C,0xC6,0xC6,0x60,0x38,0x0C,0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 84: 'T'
    {0x00,0x00,0xFF,0xDB,0x99,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 85: 'U'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 86: 'V'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00},
    // ASCII 87: 'W'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0xEE,0x6C,0x00,0x00,0x00,0x00},
    // ASCII 88: 'X'
    {0x00,0x00,0xC6,0xC6,0x6C,0x7C,0x38,0x38,0x7C,0x6C,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 89: 'Y'
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 90: 'Z'
    {0x00,0x00,0xFE,0xC6,0x86,0x0C,0x18,0x30,0x60,0xC2,0xC6,0xFE,0x00,0x00,0x00,0x00},
    // ASCII 91: '['
    {0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 92: '\\'
    {0x00,0x00,0x00,0x80,0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x06,0x02,0x00,0x00,0x00,0x00},
    // ASCII 93: ']'
    {0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 94: '^'
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 95: '_'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00},
    // ASCII 96: '`'
    {0x00,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // ASCII 97: 'a'
    {0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // ASCII 98: 'b'
    {0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 99: 'c'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 100: 'd'
    {0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // ASCII 101: 'e'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 102: 'f'
    {0x00,0x00,0x1C,0x36,0x32,0x30,0x78,0x30,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00},
    // ASCII 103: 'g'
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0x00},
    // ASCII 104: 'h'
    {0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    // ASCII 105: 'i'
    {0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 106: 'j'
    {0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00},
    // ASCII 107: 'k'
    {0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00},
    // ASCII 108: 'l'
    {0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // ASCII 109: 'm'
    {0x00,0x00,0x00,0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0xD6,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 110: 'n'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},
    // ASCII 111: 'o'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 112: 'p'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    // ASCII 113: 'q'
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00},
    // ASCII 114: 'r'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // ASCII 115: 's'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // ASCII 116: 't'
    {0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00},
    // ASCII 117: 'u'
    {0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // ASCII 118: 'v'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00},
    // ASCII 119: 'w'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0x6C,0x00,0x00,0x00,0x00},
    // ASCII 120: 'x'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    // ASCII 121: 'y'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8,0x00},
    // ASCII 122: 'z'
    {0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00},
    // ASCII 123: '{'
    {0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00},
    // ASCII 124: '|'
    {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},
    // ASCII 125: '}'
    {0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00},
    // ASCII 126: '~'
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // --- Box-drawing glyphs (indices 95-108) ---
    // 95: ─ horizontal
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 96: │ vertical
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 97: ┌ top-left
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 98: ┐ top-right
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 99: └ bottom-left
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 100: ┘ bottom-right
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 101: ├ left tee
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 102: ┤ right tee
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 103: ┬ top tee
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 104: ┴ bottom tee
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 105: ┼ cross
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    // 106: █ full block
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
    // 107: ░ light shade
    {0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55},
    // 108: ─ title bar (same as horizontal)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

// ---------------------------------------------------------------------------
// xterm256 color palette (full 256 entries, RGB)
// ---------------------------------------------------------------------------
static const uint8_t kXterm256[256][3] = {
    // 0-15: standard + bright colors
    {  0,  0,  0},{128,  0,  0},{  0,128,  0},{128,128,  0},{  0,  0,128},{128,  0,128},{  0,128,128},{192,192,192},
    {128,128,128},{255,  0,  0},{  0,255,  0},{255,255,  0},{  0,  0,255},{255,  0,255},{  0,255,255},{255,255,255},
    // 16-231: 6x6x6 color cube
    { 0, 0, 0},{ 0, 0, 95},{ 0, 0,135},{ 0, 0,175},{ 0, 0,215},{ 0, 0,255},
    { 0, 95, 0},{ 0, 95, 95},{ 0, 95,135},{ 0, 95,175},{ 0, 95,215},{ 0, 95,255},
    { 0,135, 0},{ 0,135, 95},{ 0,135,135},{ 0,135,175},{ 0,135,215},{ 0,135,255},
    { 0,175, 0},{ 0,175, 95},{ 0,175,135},{ 0,175,175},{ 0,175,215},{ 0,175,255},
    { 0,215, 0},{ 0,215, 95},{ 0,215,135},{ 0,215,175},{ 0,215,215},{ 0,215,255},
    { 0,255, 0},{ 0,255, 95},{ 0,255,135},{ 0,255,175},{ 0,255,215},{ 0,255,255},
    { 95, 0, 0},{ 95, 0, 95},{ 95, 0,135},{ 95, 0,175},{ 95, 0,215},{ 95, 0,255},
    { 95, 95, 0},{ 95, 95, 95},{ 95, 95,135},{ 95, 95,175},{ 95, 95,215},{ 95, 95,255},
    { 95,135, 0},{ 95,135, 95},{ 95,135,135},{ 95,135,175},{ 95,135,215},{ 95,135,255},
    { 95,175, 0},{ 95,175, 95},{ 95,175,135},{ 95,175,175},{ 95,175,215},{ 95,175,255},
    { 95,215, 0},{ 95,215, 95},{ 95,215,135},{ 95,215,175},{ 95,215,215},{ 95,215,255},
    { 95,255, 0},{ 95,255, 95},{ 95,255,135},{ 95,255,175},{ 95,255,215},{ 95,255,255},
    {135, 0, 0},{135, 0, 95},{135, 0,135},{135, 0,175},{135, 0,215},{135, 0,255},
    {135, 95, 0},{135, 95, 95},{135, 95,135},{135, 95,175},{135, 95,215},{135, 95,255},
    {135,135, 0},{135,135, 95},{135,135,135},{135,135,175},{135,135,215},{135,135,255},
    {135,175, 0},{135,175, 95},{135,175,135},{135,175,175},{135,175,215},{135,175,255},
    {135,215, 0},{135,215, 95},{135,215,135},{135,215,175},{135,215,215},{135,215,255},
    {135,255, 0},{135,255, 95},{135,255,135},{135,255,175},{135,255,215},{135,255,255},
    {175, 0, 0},{175, 0, 95},{175, 0,135},{175, 0,175},{175, 0,215},{175, 0,255},
    {175, 95, 0},{175, 95, 95},{175, 95,135},{175, 95,175},{175, 95,215},{175, 95,255},
    {175,135, 0},{175,135, 95},{175,135,135},{175,135,175},{175,135,215},{175,135,255},
    {175,175, 0},{175,175, 95},{175,175,135},{175,175,175},{175,175,215},{175,175,255},
    {175,215, 0},{175,215, 95},{175,215,135},{175,215,175},{175,215,215},{175,215,255},
    {175,255, 0},{175,255, 95},{175,255,135},{175,255,175},{175,255,215},{175,255,255},
    {215, 0, 0},{215, 0, 95},{215, 0,135},{215, 0,175},{215, 0,215},{215, 0,255},
    {215, 95, 0},{215, 95, 95},{215, 95,135},{215, 95,175},{215, 95,215},{215, 95,255},
    {215,135, 0},{215,135, 95},{215,135,135},{215,135,175},{215,135,215},{215,135,255},
    {215,175, 0},{215,175, 95},{215,175,135},{215,175,175},{215,175,215},{215,175,255},
    {215,215, 0},{215,215, 95},{215,215,135},{215,215,175},{215,215,215},{215,215,255},
    {215,255, 0},{215,255, 95},{215,255,135},{215,255,175},{215,255,215},{215,255,255},
    {255, 0, 0},{255, 0, 95},{255, 0,135},{255, 0,175},{255, 0,215},{255, 0,255},
    {255, 95, 0},{255, 95, 95},{255, 95,135},{255, 95,175},{255, 95,215},{255, 95,255},
    {255,135, 0},{255,135, 95},{255,135,135},{255,135,175},{255,135,215},{255,135,255},
    {255,175, 0},{255,175, 95},{255,175,135},{255,175,175},{255,175,215},{255,175,255},
    {255,215, 0},{255,215, 95},{255,215,135},{255,215,175},{255,215,215},{255,215,255},
    {255,255, 0},{255,255, 95},{255,255,135},{255,255,175},{255,255,215},{255,255,255},
    // 232-255: grayscale ramp
    {  8,  8,  8},{ 18, 18, 18},{ 28, 28, 28},{ 38, 38, 38},{ 48, 48, 48},{ 58, 58, 58},
    { 68, 68, 68},{ 78, 78, 78},{ 88, 88, 88},{ 98, 98, 98},{108,108,108},{118,118,118},
    {128,128,128},{138,138,138},{148,148,148},{158,158,158},{168,168,168},{178,178,178},
    {188,188,188},{198,198,198},{208,208,208},{218,218,218},{228,228,228},{238,238,238},
};

// ---------------------------------------------------------------------------
// TUI data structures
// ---------------------------------------------------------------------------
struct TuiCell {
    uint8_t ch = 0;  // glyph index (0=space, 1='!', ... 94='~', 95+=box-drawing)
    uint8_t fg = 15;  // xterm256 foreground
    uint8_t bg = 0;   // xterm256 background
};

static int gTuiCols = 0, gTuiRows = 0;
static std::vector<TuiCell> gTuiCells;

#ifdef GLVIEW_OPENGL_ENABLED
// TUI GL resources
static GLuint gTuiProg = 0;
static GLuint gTuiFontTex = 0;
static GLuint gTuiVAO = 0, gTuiVBO = 0, gTuiIBO = 0;
#endif

// TUI state
static bool gShowInspector = true;
static float gBgAlpha = 0.75f;  // semi-transparent background

// Terminal window state
static const int kTermMaxLines = 128;
static const int kTermDefaultRows = 7;
static std::vector<std::string> gTermLines(kTermMaxLines);
static int gTermHead = 0;
static int gTermCount = 0;
static bool gTermExpanded = false;

// Command input state
static bool gCmdInputActive = false;
static std::string gCmdInputBuf;
static int gCmdInputCursor = 0;

// FPS counter
static double gFpsLastTime = 0.0;
static int gFpsFrameCount = 0;
static float gFpsDisplay = 0.0f;

// Atlas layout: 16 glyphs per row
static const int kAtlasCols = 16;
static const int kAtlasRows = (kFontGlyphCount + kAtlasCols - 1) / kAtlasCols;
static const int kAtlasW = kAtlasCols * kFontGlyphW;  // 128
static const int kAtlasH = kAtlasRows * kFontGlyphH;  // 112

#ifdef GLVIEW_OPENGL_ENABLED
// ---------------------------------------------------------------------------
// Font texture atlas creation
// ---------------------------------------------------------------------------
static GLuint createFontTexture() {
    std::vector<uint8_t> pixels(kAtlasW * kAtlasH, 0);

    for (int g = 0; g < kFontGlyphCount; ++g) {
        int col = g % kAtlasCols;
        int row = g / kAtlasCols;
        int ox = col * kFontGlyphW;
        int oy = row * kFontGlyphH;

        for (int y = 0; y < kFontGlyphH; ++y) {
            uint8_t bits = kFontData[g][y];
            for (int x = 0; x < kFontGlyphW; ++x) {
                if (bits & (0x80 >> x)) {
                    pixels[(oy + y) * kAtlasW + (ox + x)] = 255;
                }
            }
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasW, kAtlasH, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static void tuiInitGL() {
    gTuiProg = createProgram(kTuiVS, kTuiFS);
    gTuiFontTex = createFontTexture();

    glGenVertexArrays(1, &gTuiVAO);
    glGenBuffers(1, &gTuiVBO);
    glGenBuffers(1, &gTuiIBO);
}
#endif // GLVIEW_OPENGL_ENABLED

// ---------------------------------------------------------------------------
// TUI drawing API
// ---------------------------------------------------------------------------
static uint8_t gTuiCurFg = 15;
static uint8_t gTuiCurBg = 0;

static void tuiResize(int fbW, int fbH) {
    gTuiCols = fbW / kFontGlyphW;
    gTuiRows = fbH / kFontGlyphH;
    if (gTuiCols < 1) gTuiCols = 1;
    if (gTuiRows < 1) gTuiRows = 1;
    gTuiCells.resize(gTuiCols * gTuiRows);
}

static void tuiClear() {
    TuiCell empty;
    empty.ch = 0;
    empty.fg = 15;
    empty.bg = 0;
    std::fill(gTuiCells.begin(), gTuiCells.end(), empty);
}

static void tuiSetColor(uint8_t fg, uint8_t bg) {
    gTuiCurFg = fg;
    gTuiCurBg = bg;
}

static void tuiPutChar(int col, int row, int glyphIdx) {
    if (col < 0 || col >= gTuiCols || row < 0 || row >= gTuiRows) return;
    TuiCell& c = gTuiCells[row * gTuiCols + col];
    c.ch = static_cast<uint8_t>(glyphIdx);
    c.fg = gTuiCurFg;
    c.bg = gTuiCurBg;
}

static void tuiPrint(int col, int row, const char* text) {
    for (int i = 0; text[i] != '\0'; ++i) {
        int ch = static_cast<unsigned char>(text[i]);
        int glyphIdx = 0;
        if (ch >= 32 && ch <= 126) glyphIdx = ch - 32;
        tuiPutChar(col + i, row, glyphIdx);
    }
}

static void tuiPrintf(int col, int row, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    tuiPrint(col, row, buf);
}

static void tuiFillRect(int col, int row, int w, int h) {
    for (int r = row; r < row + h; ++r)
        for (int c = col; c < col + w; ++c)
            tuiPutChar(c, r, 0); // space with current bg
}

// ---------------------------------------------------------------------------
// Terminal log — append a line to the scrollback ring buffer
// ---------------------------------------------------------------------------
static void termLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Also print to real terminal for debug
    std::printf("%s\n", buf);

    gTermLines[gTermHead] = buf;
    gTermHead = (gTermHead + 1) % kTermMaxLines;
    if (gTermCount < kTermMaxLines) gTermCount++;
}

static void tuiDrawBox(int col, int row, int w, int h) {
    if (w < 2 || h < 2) return;
    // Corners
    tuiPutChar(col, row, kBoxTL);
    tuiPutChar(col + w - 1, row, kBoxTR);
    tuiPutChar(col, row + h - 1, kBoxBL);
    tuiPutChar(col + w - 1, row + h - 1, kBoxBR);
    // Top and bottom edges
    for (int c = col + 1; c < col + w - 1; ++c) {
        tuiPutChar(c, row, kBoxH);
        tuiPutChar(c, row + h - 1, kBoxH);
    }
    // Left and right edges
    for (int r = row + 1; r < row + h - 1; ++r) {
        tuiPutChar(col, r, kBoxV);
        tuiPutChar(col + w - 1, r, kBoxV);
    }
}

// ---------------------------------------------------------------------------
// FPS counter
// ---------------------------------------------------------------------------
static void updateFPS() {
    gFpsFrameCount++;
    double now = glfwGetTime();
    double elapsed = now - gFpsLastTime;
    if (elapsed >= 0.5) {
        gFpsDisplay = static_cast<float>(gFpsFrameCount) / static_cast<float>(elapsed);
        gFpsFrameCount = 0;
        gFpsLastTime = now;
    }
}

// ---------------------------------------------------------------------------
// Terminal window — draw collapsed title bar or expanded terminal box
// ---------------------------------------------------------------------------
static void tuiComposeTerm() {
    if (gTuiCols < 20 || gTuiRows < 4) return;

    int termW = gTuiCols; // full width

    if (!gTermExpanded) {
        // If collapsed and input active, cancel input mode
        if (gCmdInputActive) {
            gCmdInputActive = false;
            gCmdInputBuf.clear();
            gCmdInputCursor = 0;
        }
        // Collapsed: single title bar row at gTuiRows - 2
        int titleRow = gTuiRows - 2;
        tuiSetColor(250, 236);
        tuiFillRect(0, titleRow, termW, 1);
        tuiPrint(1, titleRow, "[+] Terminal");
        // Fill rest of title bar with horizontal line
        tuiSetColor(240, 236);
        for (int c = 14; c < termW; ++c)
            tuiPutChar(c, titleRow, kBoxH);
    } else {
        // Expanded: box with title + content + bottom border
        int boxH = kTermDefaultRows + 2; // top border + content + bottom border
        int termTop = gTuiRows - 1 - boxH; // above help hint row
        if (termTop < 1) termTop = 1;
        int actualContentRows = gTuiRows - 1 - termTop - 2; // subtract top/bottom border and help hint
        if (actualContentRows < 1) return;

        // When input is active, steal last content row for the input line
        int scrollbackRows = actualContentRows;
        if (gCmdInputActive && actualContentRows > 1)
            scrollbackRows = actualContentRows - 1;

        // Background fill
        tuiSetColor(250, 234);
        tuiFillRect(0, termTop, termW, boxH);

        // Top border with title
        tuiSetColor(240, 234);
        tuiPutChar(0, termTop, kBoxTL);
        tuiPutChar(termW - 1, termTop, kBoxTR);
        for (int c = 1; c < termW - 1; ++c)
            tuiPutChar(c, termTop, kBoxH);
        // Title text
        tuiSetColor(250, 234);
        tuiPrint(2, termTop, "[-] Terminal");

        // Bottom border
        tuiSetColor(240, 234);
        tuiPutChar(0, termTop + actualContentRows + 1, kBoxBL);
        tuiPutChar(termW - 1, termTop + actualContentRows + 1, kBoxBR);
        for (int c = 1; c < termW - 1; ++c)
            tuiPutChar(c, termTop + actualContentRows + 1, kBoxH);

        // Left/right borders for content rows
        for (int r = 1; r <= actualContentRows; ++r) {
            tuiSetColor(240, 234);
            tuiPutChar(0, termTop + r, kBoxV);
            tuiPutChar(termW - 1, termTop + r, kBoxV);
        }

        // Content: show most recent lines from ring buffer
        tuiSetColor(252, 234);
        int innerW = termW - 4; // 2 border + 1 padding each side
        if (innerW < 1) innerW = 1;

        for (int i = 0; i < scrollbackRows; ++i) {
            // Line index from the end of buffer
            int lineIdx = gTermCount - scrollbackRows + i;
            if (lineIdx < 0) continue;

            // Map to ring buffer position
            int bufIdx = (gTermHead - gTermCount + lineIdx + kTermMaxLines) % kTermMaxLines;
            const std::string& line = gTermLines[bufIdx];

            // Truncate to fit
            int len = static_cast<int>(line.size());
            if (len > innerW) len = innerW;
            for (int c = 0; c < len; ++c) {
                int ch = static_cast<unsigned char>(line[c]);
                int glyphIdx = 0;
                if (ch >= 32 && ch <= 126) glyphIdx = ch - 32;
                tuiPutChar(2 + c, termTop + 1 + i, glyphIdx);
            }
        }

        // Draw input line when active
        if (gCmdInputActive) {
            int inputRow = termTop + 1 + scrollbackRows;
            int promptCol = 2;

            // Green "> " prompt
            tuiSetColor(10, 234); // green on dark
            tuiPutChar(promptCol, inputRow, '>' - 32);
            tuiPutChar(promptCol + 1, inputRow, ' ' - 32);

            int textStartCol = promptCol + 2;
            int visibleW = innerW - 2; // subtract prompt width
            if (visibleW < 1) visibleW = 1;

            // Horizontal scroll: if cursor past visible area, shift view
            int scrollOffset = 0;
            if (gCmdInputCursor >= visibleW)
                scrollOffset = gCmdInputCursor - visibleW + 1;

            // Draw buffer text
            tuiSetColor(252, 234);
            int bufLen = static_cast<int>(gCmdInputBuf.size());
            for (int c = 0; c < visibleW; ++c) {
                int bufIdx = scrollOffset + c;
                if (bufIdx >= bufLen) break;
                int ch = static_cast<unsigned char>(gCmdInputBuf[bufIdx]);
                int glyphIdx = 0;
                if (ch >= 32 && ch <= 126) glyphIdx = ch - 32;
                tuiPutChar(textStartCol + c, inputRow, glyphIdx);
            }

            // Draw block cursor (inverted colors)
            int cursorScreenCol = textStartCol + (gCmdInputCursor - scrollOffset);
            if (cursorScreenCol >= textStartCol && cursorScreenCol < textStartCol + visibleW) {
                int glyphIdx = 0; // space
                if (gCmdInputCursor < bufLen) {
                    int ch = static_cast<unsigned char>(gCmdInputBuf[gCmdInputCursor]);
                    if (ch >= 32 && ch <= 126) glyphIdx = ch - 32;
                }
                tuiSetColor(234, 252); // inverted: dark on light
                tuiPutChar(cursorScreenCol, inputRow, glyphIdx);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// TUI compose — write cells into the TUI buffer
// ---------------------------------------------------------------------------
static void tuiCompose() {
    tuiClear();

    if (gTuiCols < 20 || gTuiRows < 3) return; // too small

    // Find selected object
    const SceneObject* selObj = nullptr;
    for (auto& obj : gObjects) {
        if (obj.selected) { selObj = &obj; break; }
    }

    // Render mode name
    const char* modeName = "Unknown";
    switch (gRenderMode) {
        case 4: modeName = "Wireframe"; break;
        case 5: modeName = "Shading"; break;
        case 6: modeName = "Shading+Tex"; break;
        case 7: modeName = "Lighting"; break;
    }

    // --- Status bar (row 0) ---
    tuiSetColor(15, 236); // white on dark gray
    tuiFillRect(0, 0, gTuiCols, 1);
    tuiPrintf(1, 0, "FPS: %.0f", gFpsDisplay);
    tuiPrint(14, 0, modeName);

    if (selObj) {
        // Truncate name if needed
        char selBuf[64];
        snprintf(selBuf, sizeof(selBuf), "Sel: %s", selObj->name.c_str());
        int selLen = static_cast<int>(strlen(selBuf));
        int selCol = gTuiCols - selLen - 2;
        if (selCol < 28) selCol = 28;
        tuiPrint(selCol, 0, selBuf);
    }

    // --- Terminal window (above help hint) ---
    tuiComposeTerm();

    // --- Help hint (bottom row) ---
    tuiSetColor(250, 236); // light gray on dark gray
    tuiFillRect(0, gTuiRows - 1, gTuiCols, 1);
    if (gCmdInputActive)
        tuiPrint(1, gTuiRows - 1, "ESC:Cancel  Enter:Submit  Backspace:Delete");
    else
        tuiPrint(1, gTuiRows - 1, "/:Command  I:Inspector  4-7:Mode  A:FitAll  F:FitSel  ESC:Quit");

    // --- Inspector panel (right side) ---
    if (gShowInspector && selObj) {
        int panelW = 24;
        if (panelW > gTuiCols - 4) panelW = gTuiCols - 4;
        int panelH = 16;
        if (panelH > gTuiRows - 3) panelH = gTuiRows - 3;
        if (panelW < 10 || panelH < 6) return;

        int px = gTuiCols - panelW - 1;
        int py = 1;

        // Fill background
        tuiSetColor(15, 235); // white on very dark gray
        tuiFillRect(px, py, panelW, panelH);

        // Draw border
        tuiSetColor(75, 235); // blue border on dark bg
        tuiDrawBox(px, py, panelW, panelH);

        // Title
        tuiSetColor(214, 235); // orange title
        tuiPrint(px + 2, py, " Inspector ");

        // Content area starts at px+2, py+1
        int cx = px + 2;
        int cy = py + 1;
        int innerW = panelW - 4;

        tuiSetColor(15, 235);
        // Name
        tuiPrint(cx, cy, "Name:");
        cy++;
        tuiSetColor(229, 235); // yellow
        {
            char nameBuf[32];
            snprintf(nameBuf, sizeof(nameBuf), " %.20s", selObj->name.c_str());
            tuiPrint(cx, cy, nameBuf);
        }
        cy += 2;

        // Position
        tuiSetColor(15, 235);
        tuiPrint(cx, cy, "Position:");
        cy++;
        tuiSetColor(10, 235); // green
        tuiPrintf(cx, cy,   " X: %7.3f", selObj->position.x);
        cy++;
        tuiPrintf(cx, cy,   " Y: %7.3f", selObj->position.y);
        cy++;
        tuiPrintf(cx, cy,   " Z: %7.3f", selObj->position.z);
        cy += 2;

        // Scale
        tuiSetColor(15, 235);
        tuiPrint(cx, cy, "Scale:");
        cy++;
        tuiSetColor(14, 235); // cyan
        tuiPrintf(cx, cy, " %.3f %.3f %.3f", selObj->scl.x, selObj->scl.y, selObj->scl.z);
        cy += 2;

        // World BBox
        if (cy + 3 < py + panelH) {
            BBox wb = selObj->worldBBox();
            tuiSetColor(15, 235);
            tuiPrint(cx, cy, "World BBox:");
            cy++;
            tuiSetColor(213, 235); // pink
            tuiPrintf(cx, cy, " mn:%6.2f %5.2f %5.2f", wb.mn.x, wb.mn.y, wb.mn.z);
            cy++;
            tuiPrintf(cx, cy, " mx:%6.2f %5.2f %5.2f", wb.mx.x, wb.mx.y, wb.mx.z);
        }
    }
}

// ---------------------------------------------------------------------------
// TUI geometry builder and renderer
// ---------------------------------------------------------------------------
#ifdef GLVIEW_OPENGL_ENABLED
// Vertex: vec2 pos, vec2 uv, vec3 fg, vec3 bg, float bgAlpha = 11 floats
static const int kTuiVertFloats = 11;

static std::vector<float> gTuiVerts;
static std::vector<uint32_t> gTuiIndices;

static void tuiBuildGeometry(int fbW, int fbH) {
    gTuiVerts.clear();
    gTuiIndices.clear();

    uint32_t vertCount = 0;

    float atlasW = static_cast<float>(kAtlasW);
    float atlasH = static_cast<float>(kAtlasH);

    for (int row = 0; row < gTuiRows; ++row) {
        for (int col = 0; col < gTuiCols; ++col) {
            const TuiCell& cell = gTuiCells[row * gTuiCols + col];

            // Skip empty cells with black background (fully transparent)
            if (cell.ch == 0 && cell.bg == 0) continue;

            float x0 = static_cast<float>(col * kFontGlyphW);
            float y0 = static_cast<float>(row * kFontGlyphH);
            float x1 = x0 + kFontGlyphW;
            float y1 = y0 + kFontGlyphH;

            // UV from atlas
            int glyphCol = cell.ch % kAtlasCols;
            int glyphRow = cell.ch / kAtlasCols;
            float u0 = static_cast<float>(glyphCol * kFontGlyphW) / atlasW;
            float v0 = static_cast<float>(glyphRow * kFontGlyphH) / atlasH;
            float u1 = static_cast<float>((glyphCol + 1) * kFontGlyphW) / atlasW;
            float v1 = static_cast<float>((glyphRow + 1) * kFontGlyphH) / atlasH;

            // Colors
            float fr = kXterm256[cell.fg][0] / 255.0f;
            float fg = kXterm256[cell.fg][1] / 255.0f;
            float fb = kXterm256[cell.fg][2] / 255.0f;
            float br = kXterm256[cell.bg][0] / 255.0f;
            float bg = kXterm256[cell.bg][1] / 255.0f;
            float bb = kXterm256[cell.bg][2] / 255.0f;

            float alpha = gBgAlpha;

            // 4 vertices: TL, TR, BR, BL
            auto pushV = [&](float px, float py, float u, float v) {
                gTuiVerts.push_back(px); gTuiVerts.push_back(py);
                gTuiVerts.push_back(u);  gTuiVerts.push_back(v);
                gTuiVerts.push_back(fr); gTuiVerts.push_back(fg); gTuiVerts.push_back(fb);
                gTuiVerts.push_back(br); gTuiVerts.push_back(bg); gTuiVerts.push_back(bb);
                gTuiVerts.push_back(alpha);
            };

            pushV(x0, y0, u0, v0); // TL
            pushV(x1, y0, u1, v0); // TR
            pushV(x1, y1, u1, v1); // BR
            pushV(x0, y1, u0, v1); // BL

            // Two triangles: TL-TR-BR, TL-BR-BL
            gTuiIndices.push_back(vertCount + 0);
            gTuiIndices.push_back(vertCount + 1);
            gTuiIndices.push_back(vertCount + 2);
            gTuiIndices.push_back(vertCount + 0);
            gTuiIndices.push_back(vertCount + 2);
            gTuiIndices.push_back(vertCount + 3);

            vertCount += 4;
        }
    }
}

static void tuiRender(int fbW, int fbH) {
    if (gTuiIndices.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(gTuiProg);
    glUniform2f(glGetUniformLocation(gTuiProg, "uScreenSize"),
                static_cast<float>(fbW), static_cast<float>(fbH));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTuiFontTex);
    glUniform1i(glGetUniformLocation(gTuiProg, "uFontAtlas"), 0);

    glBindVertexArray(gTuiVAO);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, gTuiVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(gTuiVerts.size() * sizeof(float)),
                 gTuiVerts.data(), GL_STREAM_DRAW);

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gTuiIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(gTuiIndices.size() * sizeof(uint32_t)),
                 gTuiIndices.data(), GL_STREAM_DRAW);

    GLsizei stride = kTuiVertFloats * sizeof(float);

    // aPos (location 0): vec2
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    // aUV (location 1): vec2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // aFgColor (location 2): vec3
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // aBgColor (location 3): vec3
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(7 * sizeof(float)));
    glEnableVertexAttribArray(3);
    // aBgAlpha (location 4): float
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(10 * sizeof(float)));
    glEnableVertexAttribArray(4);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(gTuiIndices.size()),
                   GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// Pick FBO
// ---------------------------------------------------------------------------
static void createPickFBO(int width, int height) {
    if (gPickFBO) {
        glDeleteTextures(1, &gPickColorTex);
        glDeleteRenderbuffers(1, &gPickDepthRB);
        glDeleteFramebuffers(1, &gPickFBO);
    }

    gPickFBOWidth = width;
    gPickFBOHeight = height;

    glGenFramebuffers(1, &gPickFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gPickFBO);

    glGenTextures(1, &gPickColorTex);
    glBindTexture(GL_TEXTURE_2D, gPickColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPickColorTex, 0);

    glGenRenderbuffers(1, &gPickDepthRB);
    glBindRenderbuffer(GL_RENDERBUFFER, gPickDepthRB);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gPickDepthRB);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Pick FBO incomplete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Color picking
// ---------------------------------------------------------------------------
static Vec3 pickIdToColor(int id) {
    return {
        static_cast<float>((id & 0xFF)) / 255.0f,
        static_cast<float>((id >> 8) & 0xFF) / 255.0f,
        static_cast<float>((id >> 16) & 0xFF) / 255.0f
    };
}

static int colorToPickId(unsigned char r, unsigned char g, unsigned char b) {
    return static_cast<int>(r) | (static_cast<int>(g) << 8) | (static_cast<int>(b) << 16);
}

static int performColorPick(GLFWwindow* window, GLuint pickProg, const Mat4& vp,
                             double cursorX, double cursorY) {
    // Convert cursor to framebuffer coords (HiDPI)
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);

    float scaleX = static_cast<float>(fbW) / static_cast<float>(winW);
    float scaleY = static_cast<float>(fbH) / static_cast<float>(winH);
    int px = static_cast<int>(cursorX * scaleX);
    int py = fbH - 1 - static_cast<int>(cursorY * scaleY);

    // Ensure FBO matches framebuffer size
    if (fbW != gPickFBOWidth || fbH != gPickFBOHeight) {
        createPickFBO(fbW, fbH);
    }

    // Render pick pass
    glBindFramebuffer(GL_FRAMEBUFFER, gPickFBO);
    glViewport(0, 0, fbW, fbH);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(pickProg);
    GLint uModelLoc = glGetUniformLocation(pickProg, "uModel");
    GLint uVPLoc = glGetUniformLocation(pickProg, "uVP");
    GLint uPickColorLoc = glGetUniformLocation(pickProg, "uPickColor");

    glUniformMatrix4fv(uVPLoc, 1, GL_FALSE, vp.m);

    for (auto& obj : gObjects) {
        Mat4 model = obj.modelMatrix();
        glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, model.m);
        Vec3 col = pickIdToColor(obj.pickId);
        glUniform3f(uPickColorLoc, col.x, col.y, col.z);
        glBindVertexArray(obj.mesh->vao);
        glDrawArrays(GL_TRIANGLES, 0, obj.mesh->vertexCount);
    }

    // Read pixel
    unsigned char pixel[3] = {0, 0, 0};
    if (px >= 0 && px < fbW && py >= 0 && py < fbH) {
        glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Restore viewport
    glViewport(0, 0, fbW, fbH);

    int id = colorToPickId(pixel[0], pixel[1], pixel[2]);
    return id;
}

// ---------------------------------------------------------------------------
// Ray picking (SW)
// ---------------------------------------------------------------------------
static Ray screenToWorldRay(GLFWwindow* window, const Mat4& view, const Mat4& proj,
                             double cursorX, double cursorY) {
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    // Cursor to NDC
    float ndcX = (2.0f * static_cast<float>(cursorX) / static_cast<float>(winW)) - 1.0f;
    float ndcY = 1.0f - (2.0f * static_cast<float>(cursorY) / static_cast<float>(winH));

    Mat4 invVP = (proj * view).inverse();
    Vec3 nearPt = mat4TransformPoint(invVP, {ndcX, ndcY, -1.0f});
    Vec3 farPt  = mat4TransformPoint(invVP, {ndcX, ndcY,  1.0f});

    return {nearPt, normalize(farPt - nearPt)};
}

static bool rayIntersectsAABB(const Ray& ray, const BBox& box, float& tOut) {
    float tmin = -1e30f, tmax = 1e30f;

    float* mn = const_cast<float*>(&box.mn.x);
    float* mx = const_cast<float*>(&box.mx.x);
    float* orig = const_cast<float*>(&ray.origin.x);
    float* dir = const_cast<float*>(&ray.direction.x);

    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dir[i]) < 1e-8f) {
            if (orig[i] < mn[i] || orig[i] > mx[i]) return false;
        } else {
            float invD = 1.0f / dir[i];
            float t1 = (mn[i] - orig[i]) * invD;
            float t2 = (mx[i] - orig[i]) * invD;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    if (tmax < 0) return false;
    tOut = tmin >= 0 ? tmin : tmax;
    return true;
}

static int performRayPick(GLFWwindow* window, const Mat4& view, const Mat4& proj,
                           double cursorX, double cursorY) {
    Ray ray = screenToWorldRay(window, view, proj, cursorX, cursorY);

    int bestId = 0;
    float bestT = 1e30f;
    for (auto& obj : gObjects) {
        BBox wbox = obj.worldBBox();
        float t;
        if (rayIntersectsAABB(ray, wbox, t)) {
            if (t < bestT) {
                bestT = t;
                bestId = obj.pickId;
            }
        }
    }
    return bestId;
}
#endif // GLVIEW_OPENGL_ENABLED

// ---------------------------------------------------------------------------
// Fit camera to scene / selection
// ---------------------------------------------------------------------------
static void fitCameraToAll() {
    BBox fitBox;
    for (auto& obj : gObjects) {
        BBox wb = obj.worldBBox();
        fitBox.expand(wb.mn);
        fitBox.expand(wb.mx);
    }

    gCamera.target = fitBox.center();
    float rad = fitBox.radius();
    if (rad < 0.01f) rad = 1.0f;
    float fovRad = 45.0f * (3.14159265f / 180.0f);
    gCamera.distance = rad / (0.8f * std::tan(fovRad * 0.5f));
}

static void fitCameraToSelection() {
    BBox fitBox;

    bool anySelected = false;
    for (auto& obj : gObjects) {
        if (obj.selected) {
            BBox wb = obj.worldBBox();
            fitBox.expand(wb.mn);
            fitBox.expand(wb.mx);
            anySelected = true;
        }
    }

    if (!anySelected) {
        fitCameraToAll();
        return;
    }

    gCamera.target = fitBox.center();
    float rad = fitBox.radius();
    if (rad < 0.01f) rad = 1.0f;
    float fovRad = 45.0f * (3.14159265f / 180.0f);
    gCamera.distance = rad / (0.8f * std::tan(fovRad * 0.5f));
}

// ---------------------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------------------
static void framebufferSizeCb(GLFWwindow* /*window*/, int width, int height) {
    if (height == 0) height = 1;
    gAspect = static_cast<float>(width) / static_cast<float>(height);
#ifdef GLVIEW_OPENGL_ENABLED
    if (!gUseVulkan) {
        glViewport(0, 0, width, height);
        tuiResize(width, height);
    }
#endif
#ifdef GLVIEW_VULKAN_ENABLED
    if (gUseVulkan && gVkBackend) {
        gVkBackend->onResize(width, height);
    }
#endif
}

static void executeCommand(const std::string& cmd) {
    // Echo the command to terminal
    termLog("> %s", cmd.c_str());

    // Trim leading/trailing whitespace
    size_t start = cmd.find_first_not_of(' ');
    if (start == std::string::npos) return;
    size_t end = cmd.find_last_not_of(' ');
    std::string trimmed = cmd.substr(start, end - start + 1);
    if (trimmed.empty()) return;

    // Split into command and argument
    size_t spacePos = trimmed.find(' ');
    std::string verb = trimmed.substr(0, spacePos);
    std::string arg;
    if (spacePos != std::string::npos) {
        size_t argStart = trimmed.find_first_not_of(' ', spacePos);
        if (argStart != std::string::npos)
            arg = trimmed.substr(argStart);
    }

    if (verb == "clear") {
        gTermHead = 0;
        gTermCount = 0;
    } else if (verb == "mode") {
        if (arg.size() == 1 && arg[0] >= '4' && arg[0] <= '7') {
            gRenderMode = arg[0] - '0';
            const char* names[] = {"Wireframe", "Shading", "Shading+Texture", "Lighting"};
            termLog("Render mode: %s (%d)", names[gRenderMode - 4], gRenderMode);
        } else {
            termLog("Usage: mode <4-7>");
        }
    } else if (verb == "fit") {
        if (arg.empty() || arg == "all") {
            fitCameraToAll();
            termLog("Fit camera to all objects");
        } else if (arg == "sel") {
            fitCameraToSelection();
            termLog("Fit camera to selection");
        } else {
            termLog("Usage: fit [all|sel]");
        }
    } else if (verb == "help") {
        termLog("Commands:");
        termLog("  clear       - Clear terminal");
        termLog("  mode <4-7>  - Set render mode");
        termLog("  fit [all]   - Fit camera to all");
        termLog("  fit sel     - Fit camera to selection");
        termLog("  help        - Show this help");
    } else {
        termLog("Unknown command: %s", verb.c_str());
    }
}

static void charCb(GLFWwindow* /*window*/, unsigned int codepoint) {
    if (!gCmdInputActive) return;

    // Suppress the activation char ('/' or ':') when buffer is empty
    // (keyCb fires before charCb, so activation already happened)
    if (gCmdInputBuf.empty() && (codepoint == '/' || codepoint == ':'))
        return;

    // Only printable ASCII
    if (codepoint < 32 || codepoint > 126) return;

    // Buffer max length
    if (static_cast<int>(gCmdInputBuf.size()) >= 200) return;

    gCmdInputBuf.insert(gCmdInputBuf.begin() + gCmdInputCursor,
                        static_cast<char>(codepoint));
    gCmdInputCursor++;
}

static void keyCb(GLFWwindow* window, int key, int /*scancode*/, int action,
                  int mods) {
    // Track modifier state on press, repeat, and release
    bool down = (action == GLFW_PRESS || action == GLFW_REPEAT);
    if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
        gModCtrl = down;
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
        gModShift = down;
    if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT)
        gModAlt = down;

    // --- Command input mode ---
    if (gCmdInputActive) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

        if (key == GLFW_KEY_ESCAPE) {
            // Cancel input
            gCmdInputActive = false;
            gCmdInputBuf.clear();
            gCmdInputCursor = 0;
            return;
        }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            // Submit command
            std::string cmd = gCmdInputBuf;
            gCmdInputActive = false;
            gCmdInputBuf.clear();
            gCmdInputCursor = 0;
            if (!cmd.empty()) executeCommand(cmd);
            return;
        }
        if (key == GLFW_KEY_BACKSPACE) {
            if (gCmdInputCursor > 0) {
                gCmdInputBuf.erase(gCmdInputCursor - 1, 1);
                gCmdInputCursor--;
            }
            return;
        }
        if (key == GLFW_KEY_DELETE) {
            if (gCmdInputCursor < static_cast<int>(gCmdInputBuf.size()))
                gCmdInputBuf.erase(gCmdInputCursor, 1);
            return;
        }
        if (key == GLFW_KEY_LEFT) {
            if (gCmdInputCursor > 0) gCmdInputCursor--;
            return;
        }
        if (key == GLFW_KEY_RIGHT) {
            if (gCmdInputCursor < static_cast<int>(gCmdInputBuf.size()))
                gCmdInputCursor++;
            return;
        }
        if (key == GLFW_KEY_HOME) {
            gCmdInputCursor = 0;
            return;
        }
        if (key == GLFW_KEY_END) {
            gCmdInputCursor = static_cast<int>(gCmdInputBuf.size());
            return;
        }
        // Consume all other keys in input mode
        return;
    }

    // --- Normal mode ---
    if (action != GLFW_PRESS) return;

    // '/' activates command input
    if (key == GLFW_KEY_SLASH) {
        gCmdInputActive = true;
        gCmdInputBuf.clear();
        gCmdInputCursor = 0;
        gTermExpanded = true;
        return;
    }

    // Shift+';' is ':' — also activates command input
    if (key == GLFW_KEY_SEMICOLON && (mods & GLFW_MOD_SHIFT)) {
        gCmdInputActive = true;
        gCmdInputBuf.clear();
        gCmdInputCursor = 0;
        gTermExpanded = true;
        return;
    }

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (key == GLFW_KEY_4) gRenderMode = 4;
    if (key == GLFW_KEY_5) gRenderMode = 5;
    if (key == GLFW_KEY_6) gRenderMode = 6;
    if (key == GLFW_KEY_7) gRenderMode = 7;

    if (key == GLFW_KEY_A) fitCameraToAll();
    if (key == GLFW_KEY_F) fitCameraToSelection();
    if (key == GLFW_KEY_I) gShowInspector = !gShowInspector;

    if (key >= GLFW_KEY_4 && key <= GLFW_KEY_7) {
        const char* names[] = {"", "", "", "", "Wireframe", "Shading", "Shading+Texture", "Lighting"};
        termLog("Render mode: %s (%d)", names[key - GLFW_KEY_0], gRenderMode);
    }
}

static void mouseButtonCb(GLFWwindow* window, int button, int action,
                           int mods) {
    // Update modifier state from the mods bitmask (hardware-level, always reliable)
    gModCtrl  = (mods & GLFW_MOD_CONTROL) != 0;
    gModShift = (mods & GLFW_MOD_SHIFT) != 0;
    gModAlt   = (mods & GLFW_MOD_ALT) != 0;

    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);

    if (action == GLFW_PRESS) {
        gLastCursorX = cx;
        gLastCursorY = cy;
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            gLmbDown = true;
            gClickPressX = cx;
            gClickPressY = cy;
        }
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) gMmbDown = true;
        if (button == GLFW_MOUSE_BUTTON_RIGHT) gRmbDown = true;
    } else {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            gLmbDown = false;
            // Check if this was a click (not a drag): < 3px movement
            double dx = cx - gClickPressX;
            double dy = cy - gClickPressY;
            if (dx * dx + dy * dy < 9.0) {
                // Only pick on bare LMB (no modifier)
                if (!gModAlt && !gModShift && !gModCtrl) {
                    // Convert cursor pos to TUI cell coordinates
                    int fbW, fbH;
                    glfwGetFramebufferSize(window, &fbW, &fbH);
                    int winW2, winH2;
                    glfwGetWindowSize(window, &winW2, &winH2);
                    float fbScaleX = (winW2 > 0) ? static_cast<float>(fbW) / static_cast<float>(winW2) : 1.0f;
                    float fbScaleY = (winH2 > 0) ? static_cast<float>(fbH) / static_cast<float>(winH2) : 1.0f;
                    int cellCol = static_cast<int>(cx * fbScaleX) / kFontGlyphW;
                    int cellRow = static_cast<int>(cy * fbScaleY) / kFontGlyphH;

                    // Check if click is on terminal toggle icon [+]/[-]
                    int termTitleRow;
                    if (gTermExpanded) {
                        int boxH = kTermDefaultRows + 2;
                        termTitleRow = gTuiRows - 1 - boxH;
                        if (termTitleRow < 1) termTitleRow = 1;
                    } else {
                        termTitleRow = gTuiRows - 2;
                    }
                    if (cellRow == termTitleRow && cellCol >= 1 && cellCol <= 3) {
                        gTermExpanded = !gTermExpanded;
                    } else {
                        gPendingClick = true;
                        gPendingClickX = cx;
                        gPendingClickY = cy;
                    }
                }
            }
        }
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) gMmbDown = false;
        if (button == GLFW_MOUSE_BUTTON_RIGHT) gRmbDown = false;
    }
}

static void doPan(float dx, float dy) {
    Vec3 right, up;
    gCamera.getLocalAxes(right, up);
    float panSpeed = gCamera.distance * 0.002f;
    gCamera.target = gCamera.target -
                     right * (dx * panSpeed) +
                     up * (dy * panSpeed);
}

static void doDolly(float dy) {
    gCamera.distance += dy * 0.02f * gCamera.distance;
    if (gCamera.distance < 0.1f) gCamera.distance = 0.1f;
}

static void cursorPosCb(GLFWwindow* window, double xpos, double ypos) {
    float dx = static_cast<float>(xpos - gLastCursorX);
    float dy = static_cast<float>(ypos - gLastCursorY);
    gLastCursorX = xpos;
    gLastCursorY = ypos;

    // Refresh modifier state by polling (fallback for missed key events)
    gModCtrl  = gModCtrl  || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                             glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    gModShift = gModShift || glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                             glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    gModAlt   = gModAlt   || glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                             glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

    if (gLmbDown) {
        if (gModCtrl && gModShift) {
            // Ctrl+Shift+LMB = orbit
            gCamera.longitude -= dx * 0.3f;
            gCamera.latitude += dy * 0.3f;
            if (gCamera.latitude > 89.0f) gCamera.latitude = 89.0f;
            if (gCamera.latitude < -89.0f) gCamera.latitude = -89.0f;
        } else if (gModShift) {
            doPan(dx, dy);
        } else if (gModCtrl) {
            doDolly(dy);
        } else if (gModAlt) {
            gCamera.longitude -= dx * 0.3f;
            gCamera.latitude += dy * 0.3f;
            if (gCamera.latitude > 89.0f) gCamera.latitude = 89.0f;
            if (gCamera.latitude < -89.0f) gCamera.latitude = -89.0f;
        }
    }

    if (gModAlt && gMmbDown) {
        doPan(dx, dy);
    }

    if (gModAlt && gRmbDown) {
        doDolly(dy);
    }
}

static void scrollCb(GLFWwindow* /*window*/, double /*xoffset*/,
                      double yoffset) {
    gCamera.distance -= static_cast<float>(yoffset) * 0.3f * gCamera.distance;
    if (gCamera.distance < 0.1f) gCamera.distance = 0.1f;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // -----------------------------------------------------------------------
    // Parse CLI flags
    // -----------------------------------------------------------------------
    gUseVulkan = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--vulkan") == 0) {
#ifdef GLVIEW_VULKAN_ENABLED
            gUseVulkan = true;
#else
            std::fprintf(stderr, "Warning: Vulkan support not compiled in, "
                                 "falling back to OpenGL\n");
#endif
        } else if (std::strcmp(argv[i], "--opengl") == 0) {
            gUseVulkan = false;
        } else if (std::strcmp(argv[i], "--obj") == 0) {
            if (i + 1 < argc) {
                gObjFilePath = argv[++i];
            } else {
                std::fprintf(stderr, "--obj requires a file path\n");
                return EXIT_FAILURE;
            }
        } else if (argv[i][0] != '-') {
            // Positional argument: treat as OBJ file path
            gObjFilePath = argv[i];
        }
    }

    std::printf("Light3D v%s – %s viewer example\n",
                light3d::getVersionString().c_str(),
                gUseVulkan ? "Vulkan" : "OpenGL");

    // -----------------------------------------------------------------------
    // GLFW init
    // -----------------------------------------------------------------------
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    if (!gUseVulkan) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    int winW = 800, winH = 600;
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    float contentScale = 1.0f;
    if (primary) {
        float xscale = 1.0f, yscale = 1.0f;
        glfwGetMonitorContentScale(primary, &xscale, &yscale);
        contentScale = xscale;

        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        if (mode) {
            int rawW = mode->width;
            int scaledW = static_cast<int>(static_cast<float>(rawW) * xscale);
            int physicalW = rawW > scaledW ? rawW : scaledW;
            std::printf("Monitor: %dx%d, content scale: %.2f "
                        "(physical ~%d px wide)\n",
                        mode->width, mode->height, xscale, physicalW);
            if (physicalW >= 3840) {
                winW = 1600;
                winH = 1200;
            }
        }
    }
    if (contentScale > 1.0f) {
        winW = static_cast<int>(static_cast<float>(winW) / contentScale);
        winH = static_cast<int>(static_cast<float>(winH) / contentScale);
    }

    GLFWwindow* window =
        glfwCreateWindow(winW, winH, "Light3D – glview", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    {
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbH > 0)
            gAspect = static_cast<float>(fbW) / static_cast<float>(fbH);
    }

    // GLFW callbacks (shared between backends)
    glfwSetFramebufferSizeCallback(window, framebufferSizeCb);
    glfwSetKeyCallback(window, keyCb);
    glfwSetMouseButtonCallback(window, mouseButtonCb);
    glfwSetCursorPosCallback(window, cursorPosCb);
    glfwSetScrollCallback(window, scrollCb);
    glfwSetCharCallback(window, charCb);

    // -----------------------------------------------------------------------
    // Build vertex data (shared between backends)
    // -----------------------------------------------------------------------
    std::vector<float> gridVerts = buildGridVerts(10, 1.0f);
    std::vector<float> axisVerts = buildAxisVerts(2.0f);
    std::vector<float> cubeVerts = buildCubeVerts();
    std::vector<float> bboxVerts = buildBBoxWireframeVerts();

    // Local bounding box of the cube mesh (±0.5)
    BBox cubeBBox;
    cubeBBox.mn = {-0.5f, -0.5f, -0.5f};
    cubeBBox.mx = { 0.5f,  0.5f,  0.5f};

    // -----------------------------------------------------------------------
    // OBJ loading (if requested)
    // -----------------------------------------------------------------------
    std::vector<float> objVerts;
    BBox objBBox;
    bool objLoaded = false;

    if (!gObjFilePath.empty()) {
        std::printf("Loading OBJ: %s\n", gObjFilePath.c_str());
        light3d::ObjLoadResult objResult = light3d::loadObj(gObjFilePath);
        if (objResult.success) {
            ObjConvertResult conv = buildVertsFromObjResult(objResult);
            objVerts = std::move(conv.verts);
            objBBox = conv.bbox;
            objLoaded = true;
            std::printf("OBJ loaded: %zu triangles\n",
                        objVerts.size() / (kVertStride * 3));
        } else {
            std::fprintf(stderr, "Failed to load OBJ '%s': %s\n",
                         gObjFilePath.c_str(), objResult.errorMessage.c_str());
            std::fprintf(stderr, "Falling back to demo scene\n");
        }
    }

    // -----------------------------------------------------------------------
    // Vulkan path
    // -----------------------------------------------------------------------
#ifdef GLVIEW_VULKAN_ENABLED
    VulkanBackend vkBackend;
    VkMesh vkGrid, vkAxis, vkCube, vkBbox, vkObjMesh = {};

    if (gUseVulkan) {
        gVkBackend = &vkBackend;

        if (!vkBackend.init(window)) {
            std::fprintf(stderr, "Failed to initialise Vulkan backend\n");
            glfwTerminate();
            return EXIT_FAILURE;
        }

        vkGrid = vkBackend.createMesh(gridVerts);
        vkAxis = vkBackend.createMesh(axisVerts);
        vkCube = vkBackend.createMesh(cubeVerts);
        vkBbox = vkBackend.createMesh(bboxVerts);

        if (objLoaded) {
            vkObjMesh = vkBackend.createMesh(objVerts);

            SceneObject obj;
            obj.name = gObjFilePath;
            obj.meshIndex = 0;
            obj.position = {0, 0, 0};
            obj.scl = {1, 1, 1};
            obj.localBBox = objBBox;
            obj.pickId = 1;
            gObjects.push_back(obj);

            fitCameraToAll();
        } else {
            // Demo scene: 3 cubes
            {
                SceneObject obj;
                obj.name = "CubeCenter";
                obj.meshIndex = 0;
                obj.position = {0, 0.5f, 0};
                obj.scl = {1, 1, 1};
                obj.localBBox = cubeBBox;
                obj.pickId = 1;
                gObjects.push_back(obj);
            }
            {
                SceneObject obj;
                obj.name = "CubeRight";
                obj.meshIndex = 0;
                obj.position = {3, 0.5f, 0};
                obj.scl = {1, 1, 1};
                obj.localBBox = cubeBBox;
                obj.pickId = 2;
                gObjects.push_back(obj);
            }
            {
                SceneObject obj;
                obj.name = "CubeBack";
                obj.meshIndex = 0;
                obj.position = {0, 0.5f, -3};
                obj.scl = {1, 1, 1};
                obj.localBBox = cubeBBox;
                obj.pickId = 3;
                gObjects.push_back(obj);
            }
        }

        std::printf("Vulkan: rendering %zu objects\n", gObjects.size());

        // Vulkan render loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            Mat4 view = gCamera.getViewMatrix();
            float farPlane = objLoaded ? std::max(100.0f, gCamera.distance * 10.0f) : 100.0f;
            Mat4 proj = perspectiveVk(45.0f, gAspect, 0.1f, farPlane);
            Mat4 vp = proj * view;
            Mat4 identity = Mat4::identity();

            if (!vkBackend.beginFrame()) continue;

            // Grid + Axis (lines)
            vkBackend.draw(vkGrid, identity.m, vp.m, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
            vkBackend.draw(vkAxis, identity.m, vp.m, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

            // Scene objects (triangles)
            for (auto& obj : gObjects) {
                Mat4 model = obj.modelMatrix();
                VkMesh& drawMesh = objLoaded ? vkObjMesh : vkCube;
                vkBackend.draw(drawMesh, model.m, vp.m, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            }

            // Selection wireframe bbox
            for (auto& obj : gObjects) {
                if (!obj.selected) continue;
                BBox wb = obj.worldBBox();
                Vec3 center = wb.center();
                Vec3 sz = wb.size();
                Mat4 bboxModel = Mat4::translate(center) * Mat4::scale(sz);
                vkBackend.draw(vkBbox, bboxModel.m, vp.m, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
            }

            vkBackend.endFrame();
        }

        // Vulkan cleanup
        vkBackend.destroyMesh(vkGrid);
        vkBackend.destroyMesh(vkAxis);
        vkBackend.destroyMesh(vkCube);
        vkBackend.destroyMesh(vkBbox);
        if (objLoaded) vkBackend.destroyMesh(vkObjMesh);
        vkBackend.cleanup();
        gVkBackend = nullptr;

        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_SUCCESS;
    }
#endif // GLVIEW_VULKAN_ENABLED

    // -----------------------------------------------------------------------
    // OpenGL path
    // -----------------------------------------------------------------------
#ifdef GLVIEW_OPENGL_ENABLED
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // GLAD init
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialise GLAD\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::printf("OpenGL %s, %s\n", glGetString(GL_VERSION),
                glGetString(GL_RENDERER));

    // Save GL info for terminal log (logged after TUI init)
    std::string glInfoStr = std::string("OpenGL ") +
        reinterpret_cast<const char*>(glGetString(GL_VERSION)) + ", " +
        reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    // -----------------------------------------------------------------------
    // Create shader programs
    // -----------------------------------------------------------------------
    GLuint progColor = createProgram(kColorVS, kColorFS);
    GLuint progTextured = createProgram(kTexturedVS, kTexturedFS);
    GLuint progLit = createProgram(kLitVS, kLitFS);
    GLuint progPick = createProgram(kPickVS, kPickFS);

    if (!progColor || !progTextured || !progLit || !progPick) {
        std::fprintf(stderr, "Failed to create shader programs\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Uniform locations — Color
    GLint uColorModel = glGetUniformLocation(progColor, "uModel");
    GLint uColorVP    = glGetUniformLocation(progColor, "uVP");

    // Uniform locations — Textured
    GLint uTexModel   = glGetUniformLocation(progTextured, "uModel");
    GLint uTexVP      = glGetUniformLocation(progTextured, "uVP");
    GLint uTexSampler = glGetUniformLocation(progTextured, "uTexture");

    // Uniform locations — Lit
    GLint uLitModel      = glGetUniformLocation(progLit, "uModel");
    GLint uLitVP         = glGetUniformLocation(progLit, "uVP");
    GLint uLitNormMat    = glGetUniformLocation(progLit, "uNormalMatrix");
    GLint uLitLightDir   = glGetUniformLocation(progLit, "uLightDir");
    GLint uLitCameraPos  = glGetUniformLocation(progLit, "uCameraPos");
    GLint uLitLightColor = glGetUniformLocation(progLit, "uLightColor");
    GLint uLitAmbient    = glGetUniformLocation(progLit, "uAmbient");

    // -----------------------------------------------------------------------
    // Upload geometry to GL
    // -----------------------------------------------------------------------
    Mesh grid = createMesh(gridVerts);
    Mesh axis = createMesh(axisVerts);
    Mesh cubeMesh = createMesh(cubeVerts);
    Mesh bboxWire = createMesh(bboxVerts);

    // OBJ mesh — declared at main() scope so the pointer stays valid
    Mesh objMesh = {};
    if (objLoaded) {
        objMesh = createMesh(objVerts);
    }

    // -----------------------------------------------------------------------
    // Scene objects
    // -----------------------------------------------------------------------
    if (objLoaded) {
        SceneObject obj;
        obj.name = gObjFilePath;
        obj.mesh = &objMesh;
        obj.position = {0, 0, 0};
        obj.scl = {1, 1, 1};
        obj.localBBox = objBBox;
        obj.pickId = 1;
        gObjects.push_back(obj);

        fitCameraToAll();
    } else {
        // Demo scene: 3 cubes sharing the same mesh
        {
            SceneObject obj;
            obj.name = "CubeCenter";
            obj.mesh = &cubeMesh;
            obj.position = {0, 0.5f, 0};
            obj.scl = {1, 1, 1};
            obj.localBBox = cubeBBox;
            obj.pickId = 1;
            gObjects.push_back(obj);
        }
        {
            SceneObject obj;
            obj.name = "CubeRight";
            obj.mesh = &cubeMesh;
            obj.position = {3, 0.5f, 0};
            obj.scl = {1, 1, 1};
            obj.localBBox = cubeBBox;
            obj.pickId = 2;
            gObjects.push_back(obj);
        }
        {
            SceneObject obj;
            obj.name = "CubeBack";
            obj.mesh = &cubeMesh;
            obj.position = {0, 0.5f, -3};
            obj.scl = {1, 1, 1};
            obj.localBBox = cubeBBox;
            obj.pickId = 3;
            gObjects.push_back(obj);
        }
    }

    // -----------------------------------------------------------------------
    // Textures & FBO
    // -----------------------------------------------------------------------
    gCheckerTex = createCheckerTexture();

    {
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        createPickFBO(fbW, fbH);
        tuiResize(fbW, fbH);
    }

    // TUI overlay init
    tuiInitGL();
    gFpsLastTime = glfwGetTime();

    // Light direction for lit mode
    Vec3 lightDir = normalize({0.5f, 1.0f, 0.3f});

    glEnable(GL_DEPTH_TEST);

    termLog("Light3D v%s - OpenGL viewer example",
            light3d::getVersionString().c_str());
    termLog("%s", glInfoStr.c_str());
    if (objLoaded) {
        termLog("Loaded: %s (%zu tris)", gObjFilePath.c_str(),
                objVerts.size() / (kVertStride * 3));
    } else {
        termLog("Demo scene (3 cubes)");
    }
    termLog("Controls: 4=Wireframe, 5=Shading, 6=Shading+Tex, 7=Lighting");
    termLog("  LMB click=Select, F=Fit to selection");
    termLog("  Alt+LMB=Orbit, Shift+LMB=Pan, Ctrl+LMB=Dolly, Scroll=Dolly");

    // -----------------------------------------------------------------------
    // Render loop
    // -----------------------------------------------------------------------
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        Mat4 view = gCamera.getViewMatrix();
        float farPlane = objLoaded ? std::max(100.0f, gCamera.distance * 10.0f) : 100.0f;
        Mat4 proj = perspective(45.0f, gAspect, 0.1f, farPlane);
        Mat4 vp = proj * view;

        // Handle pending click (picking)
        if (gPendingClick) {
            gPendingClick = false;

            int colorId = performColorPick(window, progPick, vp, gPendingClickX, gPendingClickY);
            int rayId = performRayPick(window, view, proj, gPendingClickX, gPendingClickY);

            // Use color pick as authoritative, ray as fallback
            int pickId = colorId > 0 ? colorId : rayId;

            // Deselect all
            for (auto& obj : gObjects) obj.selected = false;

            // Select hit object
            if (pickId > 0) {
                for (auto& obj : gObjects) {
                    if (obj.pickId == pickId) {
                        obj.selected = true;
                        termLog("Pick at (%.0f, %.0f): color=%d ray=%d -> '%s'",
                                gPendingClickX, gPendingClickY, colorId, rayId,
                                obj.name.c_str());
                        break;
                    }
                }
            } else {
                termLog("Pick at (%.0f, %.0f): color=%d ray=%d -> deselected",
                        gPendingClickX, gPendingClickY, colorId, rayId);
            }
        }

        // Clear
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- Draw grid + axis (always GL_LINES, gProgColor, identity model) ----
        glUseProgram(progColor);
        glUniformMatrix4fv(uColorVP, 1, GL_FALSE, vp.m);
        Mat4 identity = Mat4::identity();
        glUniformMatrix4fv(uColorModel, 1, GL_FALSE, identity.m);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // reset for lines
        glBindVertexArray(grid.vao);
        glDrawArrays(GL_LINES, 0, grid.vertexCount);

        glBindVertexArray(axis.vao);
        glDrawArrays(GL_LINES, 0, axis.vertexCount);

        // ---- Draw scene objects (mode-appropriate shader) ----
        if (gRenderMode == 4) {
            // Wireframe
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glUseProgram(progColor);
            glUniformMatrix4fv(uColorVP, 1, GL_FALSE, vp.m);
            for (auto& obj : gObjects) {
                Mat4 model = obj.modelMatrix();
                glUniformMatrix4fv(uColorModel, 1, GL_FALSE, model.m);
                glBindVertexArray(obj.mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, obj.mesh->vertexCount);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        } else if (gRenderMode == 5) {
            // Shading (per-face vertex color)
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUseProgram(progColor);
            glUniformMatrix4fv(uColorVP, 1, GL_FALSE, vp.m);
            for (auto& obj : gObjects) {
                Mat4 model = obj.modelMatrix();
                glUniformMatrix4fv(uColorModel, 1, GL_FALSE, model.m);
                glBindVertexArray(obj.mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, obj.mesh->vertexCount);
            }

        } else if (gRenderMode == 6) {
            // Shading + Texture
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUseProgram(progTextured);
            glUniformMatrix4fv(uTexVP, 1, GL_FALSE, vp.m);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gCheckerTex);
            glUniform1i(uTexSampler, 0);
            for (auto& obj : gObjects) {
                Mat4 model = obj.modelMatrix();
                glUniformMatrix4fv(uTexModel, 1, GL_FALSE, model.m);
                glBindVertexArray(obj.mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, obj.mesh->vertexCount);
            }

        } else if (gRenderMode == 7) {
            // Lighting (Blinn-Phong)
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUseProgram(progLit);
            glUniformMatrix4fv(uLitVP, 1, GL_FALSE, vp.m);
            glUniform3f(uLitLightDir, lightDir.x, lightDir.y, lightDir.z);
            Vec3 camPos = gCamera.getEyePosition();
            glUniform3f(uLitCameraPos, camPos.x, camPos.y, camPos.z);
            glUniform3f(uLitLightColor, 1.0f, 1.0f, 1.0f);
            glUniform1f(uLitAmbient, 0.15f);

            for (auto& obj : gObjects) {
                Mat4 model = obj.modelMatrix();
                glUniformMatrix4fv(uLitModel, 1, GL_FALSE, model.m);
                float normMat[9];
                extractNormalMatrix(model, normMat);
                glUniformMatrix3fv(uLitNormMat, 1, GL_FALSE, normMat);
                glBindVertexArray(obj.mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, obj.mesh->vertexCount);
            }
        }

        // ---- Draw selection wireframe bbox ----
        glUseProgram(progColor);
        glUniformMatrix4fv(uColorVP, 1, GL_FALSE, vp.m);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        for (auto& obj : gObjects) {
            if (!obj.selected) continue;
            BBox wb = obj.worldBBox();
            Vec3 center = wb.center();
            Vec3 sz = wb.size();
            // Scale unit wireframe cube (-0.5..0.5) to match world bbox
            Mat4 bboxModel = Mat4::translate(center) * Mat4::scale(sz);
            glUniformMatrix4fv(uColorModel, 1, GL_FALSE, bboxModel.m);
            glBindVertexArray(bboxWire.vao);
            glDrawArrays(GL_LINES, 0, bboxWire.vertexCount);
        }

        // ---- TUI overlay ----
        {
            int fbW, fbH;
            glfwGetFramebufferSize(window, &fbW, &fbH);
            updateFPS();
            tuiCompose();
            tuiBuildGeometry(fbW, fbH);
            tuiRender(fbW, fbH);
        }

        glfwSwapBuffers(window);
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    destroyMesh(grid);
    destroyMesh(axis);
    destroyMesh(cubeMesh);  // shared by all 3 objects, destroy once
    destroyMesh(bboxWire);
    if (objLoaded) destroyMesh(objMesh);

    glDeleteProgram(progColor);
    glDeleteProgram(progTextured);
    glDeleteProgram(progLit);
    glDeleteProgram(progPick);

    glDeleteTextures(1, &gCheckerTex);
    glDeleteTextures(1, &gPickColorTex);
    glDeleteRenderbuffers(1, &gPickDepthRB);
    glDeleteFramebuffers(1, &gPickFBO);

    // TUI cleanup
    glDeleteProgram(gTuiProg);
    glDeleteTextures(1, &gTuiFontTex);
    glDeleteVertexArrays(1, &gTuiVAO);
    glDeleteBuffers(1, &gTuiVBO);
    glDeleteBuffers(1, &gTuiIBO);
#endif // GLVIEW_OPENGL_ENABLED

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
