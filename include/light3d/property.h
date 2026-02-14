#pragma once

#include <string>

namespace light3d {

enum class PropertyType {
    Float,
    Vec3,
    Vec4,
    Quat,
    Mat4,
    Bool,
    Int,
    String
};

struct PropertyBinding {
    std::string name;
    PropertyType type;
    void* data = nullptr;
};

} // namespace light3d
