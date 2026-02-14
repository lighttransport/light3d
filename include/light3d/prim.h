#pragma once

#include <string>
#include <vector>

namespace light3d {

using SdfPath = std::string;

enum class PrimType {
    Prim,
    Xform,
    Mesh,
    Skeleton,
    SkelAnimation
};

enum class Visibility {
    Inherited,
    Visible,
    Invisible
};

class Prim {
public:
    explicit Prim(const SdfPath& path, const std::string& name);
    virtual ~Prim() = default;

    const SdfPath& getPath() const { return path_; }
    const std::string& getName() const { return name_; }
    virtual PrimType getType() const { return PrimType::Prim; }

    Prim* getParent() const { return parent_; }
    const std::vector<Prim*>& getChildren() const { return children_; }

    void setVisibility(Visibility v) { visibility_ = v; }
    Visibility getVisibility() const { return visibility_; }
    bool isVisible() const;

private:
    friend class Stage;
    void setParent(Prim* parent) { parent_ = parent; }
    void addChild(Prim* child) { children_.push_back(child); }

    SdfPath path_;
    std::string name_;
    Prim* parent_ = nullptr;
    std::vector<Prim*> children_;
    Visibility visibility_ = Visibility::Inherited;
};

} // namespace light3d
