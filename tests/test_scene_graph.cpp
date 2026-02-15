#include "test_helpers.h"
#include <light3d/stage.h>
#include <light3d/xform.h>
#include <light3d/mesh_data.h>
#include <light3d/math.h>

using namespace light3d;

static constexpr float kEps = 1e-5f;

// --- Stage basics ---

TEST(stage_empty) {
    Stage stage;
    ASSERT_EQ(stage.primCount(), 0u);
    ASSERT_EQ(stage.getRootPrims().size(), 0u);
}

TEST(stage_define_prim) {
    Stage stage;
    Xform* x = stage.definePrim<Xform>("/root");
    ASSERT(x != nullptr);
    ASSERT_EQ(x->getPath(), std::string("/root"));
    ASSERT_EQ(x->getName(), std::string("root"));
    ASSERT_EQ(stage.primCount(), 1u);
}

TEST(stage_define_nested_creates_parents) {
    Stage stage;
    Xform* leaf = stage.definePrim<Xform>("/a/b/c");
    ASSERT(leaf != nullptr);
    ASSERT_EQ(leaf->getPath(), std::string("/a/b/c"));

    // Parent prims should exist
    Prim* a = stage.getPrimAtPath("/a");
    Prim* b = stage.getPrimAtPath("/a/b");
    ASSERT(a != nullptr);
    ASSERT(b != nullptr);
    ASSERT_EQ(a->getChildren().size(), 1u);
    ASSERT_EQ(b->getChildren().size(), 1u);
}

TEST(stage_get_prim_at_path) {
    Stage stage;
    stage.definePrim<Xform>("/foo");
    Prim* p = stage.getPrimAtPath("/foo");
    ASSERT(p != nullptr);
    ASSERT_EQ(p->getPath(), std::string("/foo"));

    Prim* missing = stage.getPrimAtPath("/bar");
    ASSERT(missing == nullptr);
}

TEST(stage_root_prims) {
    Stage stage;
    stage.definePrim<Xform>("/a");
    stage.definePrim<Xform>("/b");
    ASSERT_EQ(stage.getRootPrims().size(), 2u);
}

// --- Prim hierarchy ---

TEST(prim_parent_child) {
    Stage stage;
    stage.definePrim<Xform>("/parent/child");
    Prim* parent = stage.getPrimAtPath("/parent");
    Prim* child = stage.getPrimAtPath("/parent/child");
    ASSERT(parent != nullptr);
    ASSERT(child != nullptr);
    ASSERT_EQ(child->getParent(), parent);
    ASSERT_EQ(parent->getChildren().size(), 1u);
    ASSERT_EQ(parent->getChildren()[0], child);
}

TEST(prim_type) {
    Stage stage;
    Xform* x = stage.definePrim<Xform>("/xf");
    MeshPrim* m = stage.definePrim<MeshPrim>("/mesh");
    ASSERT_EQ(static_cast<int>(x->getType()), static_cast<int>(PrimType::Xform));
    ASSERT_EQ(static_cast<int>(m->getType()), static_cast<int>(PrimType::Mesh));
}

// --- Traverse ---

TEST(stage_traverse) {
    Stage stage;
    stage.definePrim<Xform>("/a");
    stage.definePrim<Xform>("/a/b");
    stage.definePrim<Xform>("/a/c");

    int count = 0;
    stage.traverse([&](Prim&) { count++; });
    ASSERT_EQ(count, 3);
}

// --- Visibility ---

TEST(prim_visibility_default) {
    Stage stage;
    Xform* x = stage.definePrim<Xform>("/v");
    ASSERT_EQ(static_cast<int>(x->getVisibility()), static_cast<int>(Visibility::Inherited));
    ASSERT(x->isVisible());
}

TEST(prim_visibility_invisible) {
    Stage stage;
    Xform* x = stage.definePrim<Xform>("/v");
    x->setVisibility(Visibility::Invisible);
    ASSERT(!x->isVisible());
}

// --- Xform transforms ---

TEST(xform_default_translation) {
    Stage stage;
    Xform* x = stage.definePrim<Xform>("/x");
    Vec3 t = x->getTranslation();
    ASSERT_NEAR(t.x, 0.0f, kEps);
    ASSERT_NEAR(t.y, 0.0f, kEps);
    ASSERT_NEAR(t.z, 0.0f, kEps);
}

TEST(xform_set_translation) {
    Stage stage;
    Xform* x = stage.definePrim<Xform>("/x");
    x->setTranslation(Vec3(1, 2, 3));
    Mat4 local = x->getLocalMatrix();
    Vec3 p = transformPoint(local, Vec3(0, 0, 0));
    ASSERT_NEAR(p.x, 1.0f, kEps);
    ASSERT_NEAR(p.y, 2.0f, kEps);
    ASSERT_NEAR(p.z, 3.0f, kEps);
}

TEST(xform_world_transforms) {
    Stage stage;
    Xform* parent = stage.definePrim<Xform>("/parent");
    Xform* child = stage.definePrim<Xform>("/parent/child");
    parent->setTranslation(Vec3(10, 0, 0));
    child->setTranslation(Vec3(0, 5, 0));

    stage.updateWorldTransforms();

    // Child world position should be parent + child = (10, 5, 0)
    Vec3 wp = transformPoint(child->getWorldMatrix(), Vec3(0, 0, 0));
    ASSERT_NEAR(wp.x, 10.0f, kEps);
    ASSERT_NEAR(wp.y, 5.0f, kEps);
    ASSERT_NEAR(wp.z, 0.0f, kEps);
}

int main() {
    std::printf("=== Scene Graph Tests ===\n");
    RUN_TESTS();
}
