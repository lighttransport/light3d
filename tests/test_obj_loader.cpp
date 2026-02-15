#include "test_helpers.h"
#include <light3d/obj_loader.h>
#include <light3d/math.h>
#include <cstdio>
#include <fstream>
#include <string>

using namespace light3d;

// Write a temporary OBJ file, return its path.
static std::string writeTempObj(const char* filename, const char* content) {
    std::string path = std::string("/tmp/light3d_test_") + filename;
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

// --- Basic loading ---

TEST(obj_load_triangle) {
    std::string path = writeTempObj("tri.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vn 0 0 1\n"
        "vn 0 0 1\n"
        "vn 0 0 1\n"
        "f 1//1 2//2 3//3\n"
    );
    ObjLoadResult r = loadObj(path);
    ASSERT(r.success);
    ASSERT_EQ(r.geometry.faceCount(), 1u);
    ASSERT_EQ(r.geometry.faceVertexCounts[0], 3);
    ASSERT(r.geometry.vertexCount() > 0);
}

TEST(obj_load_quad) {
    std::string path = writeTempObj("quad.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "f 1 2 3 4\n"
    );
    ObjLoadResult r = loadObj(path);
    ASSERT(r.success);
    ASSERT_EQ(r.geometry.faceCount(), 1u);
    ASSERT_EQ(r.geometry.faceVertexCounts[0], 4);
}

TEST(obj_load_multiple_faces) {
    std::string path = writeTempObj("multi.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "v 2 0 0\n"
        "v 2 1 0\n"
        "f 1 2 3\n"
        "f 3 4 1\n"
        "f 2 5 6 3\n"
    );
    ObjLoadResult r = loadObj(path);
    ASSERT(r.success);
    ASSERT_EQ(r.geometry.faceCount(), 3u);
    ASSERT_EQ(r.geometry.faceVertexCounts[0], 3);
    ASSERT_EQ(r.geometry.faceVertexCounts[1], 3);
    ASSERT_EQ(r.geometry.faceVertexCounts[2], 4);
}

TEST(obj_load_with_uvs) {
    std::string path = writeTempObj("uvs.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "f 1/1 2/2 3/3\n"
    );
    ObjLoadResult r = loadObj(path);
    ASSERT(r.success);
    ASSERT(r.geometry.uvs.size() > 0);
}

// --- Error handling ---

TEST(obj_load_nonexistent) {
    ObjLoadResult r = loadObj("/tmp/light3d_test_nonexistent_file.obj");
    ASSERT(!r.success);
    ASSERT(r.errorMessage.size() > 0);
}

TEST(obj_load_empty) {
    std::string path = writeTempObj("empty.obj", "# empty file\n");
    ObjLoadResult r = loadObj(path);
    // An empty OBJ with no faces should still succeed (or gracefully handle)
    // but have zero geometry
    ASSERT_EQ(r.geometry.faceCount(), 0u);
}

// --- Negative indices (Blender-style) ---

TEST(obj_negative_indices) {
    std::string path = writeTempObj("neg.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f -3 -2 -1\n"
    );
    ObjLoadResult r = loadObj(path);
    ASSERT(r.success);
    ASSERT_EQ(r.geometry.faceCount(), 1u);
    ASSERT_EQ(r.geometry.faceVertexCounts[0], 3);
}

int main() {
    std::printf("=== OBJ Loader Tests ===\n");
    RUN_TESTS();
}
