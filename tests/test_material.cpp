#include "test_helpers.h"
#include <light3d/material.h>
#include <light3d/math.h>

using namespace light3d;

static constexpr float kEps = 1e-5f;

// --- MaterialLibrary ---

TEST(material_library_add_and_get) {
    MaterialLibrary lib;
    Material mat;
    mat.name = "red";
    mat.baseColor = Vec3(1, 0, 0);
    int id = lib.addMaterial(mat);

    const Material* got = lib.getMaterial(id);
    ASSERT(got != nullptr);
    ASSERT_NEAR(got->baseColor.x, 1.0f, kEps);
    ASSERT_NEAR(got->baseColor.y, 0.0f, kEps);
    ASSERT_EQ(got->name, std::string("red"));
}

TEST(material_library_find_by_name) {
    MaterialLibrary lib;
    Material m1;
    m1.name = "shiny";
    lib.addMaterial(m1);

    Material m2;
    m2.name = "matte";
    lib.addMaterial(m2);

    const Material* found = lib.findByName("matte");
    ASSERT(found != nullptr);
    ASSERT_EQ(found->name, std::string("matte"));

    const Material* missing = lib.findByName("glass");
    ASSERT(missing == nullptr);
}

TEST(material_library_count) {
    MaterialLibrary lib;
    ASSERT_EQ(lib.count(), 0);

    Material m;
    m.name = "a";
    lib.addMaterial(m);
    m.name = "b";
    lib.addMaterial(m);
    ASSERT_EQ(lib.count(), 2);
}

TEST(material_library_get_invalid_id) {
    MaterialLibrary lib;
    ASSERT(lib.getMaterial(0) == nullptr);
    ASSERT(lib.getMaterial(-1) == nullptr);
    ASSERT(lib.getMaterial(999) == nullptr);
}

// --- Material defaults ---

TEST(material_defaults) {
    Material mat;
    ASSERT_NEAR(mat.metallic, 0.0f, kEps);
    ASSERT_NEAR(mat.roughness, 0.5f, kEps);
    ASSERT_NEAR(mat.alpha, 1.0f, kEps);
    ASSERT_EQ(static_cast<int>(mat.alphaMode), static_cast<int>(AlphaMode::Opaque));
    ASSERT_EQ(mat.baseColorTexture, -1);
}

// --- Pack materials ---

TEST(pack_materials_buffer_size) {
    MaterialLibrary lib;
    Material m;
    m.name = "a";
    lib.addMaterial(m);
    m.name = "b";
    lib.addMaterial(m);

    std::vector<float> packed = packMaterialsToBuffer(lib);
    ASSERT_EQ(packed.size(), static_cast<size_t>(2 * kPackedMaterialFloats));
}

TEST(pack_materials_base_color) {
    MaterialLibrary lib;
    Material m;
    m.name = "test";
    m.baseColor = Vec3(0.5f, 0.6f, 0.7f);
    m.metallic = 0.3f;
    lib.addMaterial(m);

    std::vector<float> packed = packMaterialsToBuffer(lib);
    // First vec4: (baseColor.rgb, metallic)
    ASSERT_NEAR(packed[0], 0.5f, kEps);
    ASSERT_NEAR(packed[1], 0.6f, kEps);
    ASSERT_NEAR(packed[2], 0.7f, kEps);
    ASSERT_NEAR(packed[3], 0.3f, kEps);
}

int main() {
    std::printf("=== Material Tests ===\n");
    RUN_TESTS();
}
