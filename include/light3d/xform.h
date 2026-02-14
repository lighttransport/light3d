#pragma once

#include "prim.h"
#include "math.h"
#include "property.h"
#include <vector>

namespace light3d {

class Xform : public Prim {
public:
    Xform(const SdfPath& path, const std::string& name);
    ~Xform() override = default;

    PrimType getType() const override { return PrimType::Xform; }

    // TRS accessors
    const Vec3& getTranslation() const { return translation_; }
    void setTranslation(const Vec3& t) { translation_ = t; localDirty_ = true; }

    const Quat& getRotation() const { return rotation_; }
    void setRotation(const Quat& r) { rotation_ = r; localDirty_ = true; }

    const Vec3& getScale() const { return scale_; }
    void setScale(const Vec3& s) { scale_ = s; localDirty_ = true; }

    Mat4 getLocalMatrix() const;

    const Mat4& getWorldMatrix() const { return worldMatrix_; }
    void setWorldMatrix(const Mat4& m) { worldMatrix_ = m; }

    const std::vector<PropertyBinding>& getPropertyBindings() const { return bindings_; }

protected:
    void registerProperties();

    Vec3 translation_{0.0f, 0.0f, 0.0f};
    Quat rotation_ = Quat::identity();
    Vec3 scale_{1.0f, 1.0f, 1.0f};

    mutable bool localDirty_ = true;
    mutable Mat4 localMatrix_;
    Mat4 worldMatrix_ = Mat4::identity();

    std::vector<PropertyBinding> bindings_;
};

} // namespace light3d
