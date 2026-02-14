#include "light3d/xform.h"

namespace light3d {

Xform::Xform(const SdfPath& path, const std::string& name)
    : Prim(path, name) {
    registerProperties();
}

Mat4 Xform::getLocalMatrix() const {
    if (localDirty_) {
        localMatrix_ = Mat4::trs(translation_, rotation_, scale_);
        localDirty_ = false;
    }
    return localMatrix_;
}

void Xform::registerProperties() {
    bindings_.push_back({"translation", PropertyType::Vec3, &translation_});
    bindings_.push_back({"rotation", PropertyType::Quat, &rotation_});
    bindings_.push_back({"scale", PropertyType::Vec3, &scale_});
}

} // namespace light3d
