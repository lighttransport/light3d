#include "test_helpers.h"
#include <light3d/math.h>

using namespace light3d;

static constexpr float kEps = 1e-5f;

// --- Vec3 ---

TEST(vec3_add) {
    Vec3 a(1, 2, 3), b(4, 5, 6);
    Vec3 c = a + b;
    ASSERT_NEAR(c.x, 5.0f, kEps);
    ASSERT_NEAR(c.y, 7.0f, kEps);
    ASSERT_NEAR(c.z, 9.0f, kEps);
}

TEST(vec3_sub) {
    Vec3 a(5, 7, 9), b(1, 2, 3);
    Vec3 c = a - b;
    ASSERT_NEAR(c.x, 4.0f, kEps);
    ASSERT_NEAR(c.y, 5.0f, kEps);
    ASSERT_NEAR(c.z, 6.0f, kEps);
}

TEST(vec3_scale) {
    Vec3 v(1, 2, 3);
    Vec3 r = v * 2.0f;
    ASSERT_NEAR(r.x, 2.0f, kEps);
    ASSERT_NEAR(r.y, 4.0f, kEps);
    ASSERT_NEAR(r.z, 6.0f, kEps);
}

TEST(vec3_negate) {
    Vec3 v(1, -2, 3);
    Vec3 r = -v;
    ASSERT_NEAR(r.x, -1.0f, kEps);
    ASSERT_NEAR(r.y, 2.0f, kEps);
    ASSERT_NEAR(r.z, -3.0f, kEps);
}

TEST(vec3_dot) {
    Vec3 a(1, 0, 0), b(0, 1, 0);
    ASSERT_NEAR(dot(a, b), 0.0f, kEps);
    ASSERT_NEAR(dot(a, a), 1.0f, kEps);
}

TEST(vec3_cross) {
    Vec3 x(1, 0, 0), y(0, 1, 0);
    Vec3 z = cross(x, y);
    ASSERT_NEAR(z.x, 0.0f, kEps);
    ASSERT_NEAR(z.y, 0.0f, kEps);
    ASSERT_NEAR(z.z, 1.0f, kEps);
}

TEST(vec3_length) {
    Vec3 v(3, 4, 0);
    ASSERT_NEAR(length(v), 5.0f, kEps);
}

TEST(vec3_normalize) {
    Vec3 v(0, 0, 5);
    Vec3 n = normalize(v);
    ASSERT_NEAR(n.x, 0.0f, kEps);
    ASSERT_NEAR(n.y, 0.0f, kEps);
    ASSERT_NEAR(n.z, 1.0f, kEps);
}

TEST(vec3_normalize_zero) {
    Vec3 v(0, 0, 0);
    Vec3 n = normalize(v);
    ASSERT_NEAR(n.x, 0.0f, kEps);
    ASSERT_NEAR(n.y, 0.0f, kEps);
    ASSERT_NEAR(n.z, 0.0f, kEps);
}

TEST(vec3_lerp) {
    Vec3 a(0, 0, 0), b(10, 20, 30);
    Vec3 mid = lerp(a, b, 0.5f);
    ASSERT_NEAR(mid.x, 5.0f, kEps);
    ASSERT_NEAR(mid.y, 10.0f, kEps);
    ASSERT_NEAR(mid.z, 15.0f, kEps);
}

// --- Quat ---

TEST(quat_identity) {
    Quat q = Quat::identity();
    ASSERT_NEAR(q.x, 0.0f, kEps);
    ASSERT_NEAR(q.y, 0.0f, kEps);
    ASSERT_NEAR(q.z, 0.0f, kEps);
    ASSERT_NEAR(q.w, 1.0f, kEps);
}

TEST(quat_conjugate) {
    Quat q(1, 2, 3, 4);
    Quat c = q.conjugate();
    ASSERT_NEAR(c.x, -1.0f, kEps);
    ASSERT_NEAR(c.y, -2.0f, kEps);
    ASSERT_NEAR(c.z, -3.0f, kEps);
    ASSERT_NEAR(c.w, 4.0f, kEps);
}

TEST(quat_multiply_identity) {
    Quat id = Quat::identity();
    Quat q(0.1f, 0.2f, 0.3f, 0.9f);
    q = q.normalized();
    Quat r = id * q;
    ASSERT_NEAR(r.x, q.x, kEps);
    ASSERT_NEAR(r.y, q.y, kEps);
    ASSERT_NEAR(r.z, q.z, kEps);
    ASSERT_NEAR(r.w, q.w, kEps);
}

TEST(quat_from_axis_angle) {
    // 90 degrees around Y
    float angle = 3.14159265f * 0.5f;
    Quat q = Quat::fromAxisAngle(Vec3(0, 1, 0), angle);
    float expected_w = std::cos(angle * 0.5f);
    float expected_y = std::sin(angle * 0.5f);
    ASSERT_NEAR(q.x, 0.0f, kEps);
    ASSERT_NEAR(q.y, expected_y, kEps);
    ASSERT_NEAR(q.z, 0.0f, kEps);
    ASSERT_NEAR(q.w, expected_w, kEps);
}

// --- Mat4 ---

TEST(mat4_identity) {
    Mat4 m = Mat4::identity();
    ASSERT_NEAR(m.m[0], 1.0f, kEps);
    ASSERT_NEAR(m.m[5], 1.0f, kEps);
    ASSERT_NEAR(m.m[10], 1.0f, kEps);
    ASSERT_NEAR(m.m[15], 1.0f, kEps);
    ASSERT_NEAR(m.m[1], 0.0f, kEps);
    ASSERT_NEAR(m.m[4], 0.0f, kEps);
}

TEST(mat4_multiply_identity) {
    Mat4 a = Mat4::translate(Vec3(1, 2, 3));
    Mat4 id = Mat4::identity();
    Mat4 r = a * id;
    for (int i = 0; i < 16; ++i)
        ASSERT_NEAR(r.m[i], a.m[i], kEps);
}

TEST(mat4_translate) {
    Mat4 t = Mat4::translate(Vec3(5, 10, 15));
    ASSERT_NEAR(t.m[12], 5.0f, kEps);
    ASSERT_NEAR(t.m[13], 10.0f, kEps);
    ASSERT_NEAR(t.m[14], 15.0f, kEps);
}

TEST(mat4_scale) {
    Mat4 s = Mat4::scale(Vec3(2, 3, 4));
    ASSERT_NEAR(s.m[0], 2.0f, kEps);
    ASSERT_NEAR(s.m[5], 3.0f, kEps);
    ASSERT_NEAR(s.m[10], 4.0f, kEps);
}

TEST(mat4_transform_point) {
    Mat4 t = Mat4::translate(Vec3(1, 2, 3));
    Vec3 p(0, 0, 0);
    Vec3 r = transformPoint(t, p);
    ASSERT_NEAR(r.x, 1.0f, kEps);
    ASSERT_NEAR(r.y, 2.0f, kEps);
    ASSERT_NEAR(r.z, 3.0f, kEps);
}

TEST(mat4_transform_vector_ignores_translation) {
    Mat4 t = Mat4::translate(Vec3(100, 200, 300));
    Vec3 v(1, 0, 0);
    Vec3 r = transformVector(t, v);
    ASSERT_NEAR(r.x, 1.0f, kEps);
    ASSERT_NEAR(r.y, 0.0f, kEps);
    ASSERT_NEAR(r.z, 0.0f, kEps);
}

TEST(mat4_inverse) {
    Mat4 t = Mat4::translate(Vec3(3, 5, 7));
    Mat4 inv = t.inverse();
    Mat4 r = t * inv;
    // Should be identity
    ASSERT_NEAR(r.m[0], 1.0f, kEps);
    ASSERT_NEAR(r.m[5], 1.0f, kEps);
    ASSERT_NEAR(r.m[10], 1.0f, kEps);
    ASSERT_NEAR(r.m[15], 1.0f, kEps);
    ASSERT_NEAR(r.m[12], 0.0f, kEps);
    ASSERT_NEAR(r.m[13], 0.0f, kEps);
    ASSERT_NEAR(r.m[14], 0.0f, kEps);
}

TEST(mat4_trs) {
    Mat4 m = Mat4::trs(Vec3(1, 2, 3), Quat::identity(), Vec3(1, 1, 1));
    Vec3 p = transformPoint(m, Vec3(0, 0, 0));
    ASSERT_NEAR(p.x, 1.0f, kEps);
    ASSERT_NEAR(p.y, 2.0f, kEps);
    ASSERT_NEAR(p.z, 3.0f, kEps);
}

int main() {
    std::printf("=== Math Tests ===\n");
    RUN_TESTS();
}
