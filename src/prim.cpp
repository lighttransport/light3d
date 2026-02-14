#include "light3d/prim.h"

namespace light3d {

Prim::Prim(const SdfPath& path, const std::string& name)
    : path_(path), name_(name) {}

bool Prim::isVisible() const {
    if (visibility_ == Visibility::Invisible) return false;
    if (visibility_ == Visibility::Visible) return true;
    // Inherited: walk up parents
    if (parent_) return parent_->isVisible();
    return true;
}

} // namespace light3d
