#pragma once

#include "xform.h"

namespace light3d {

enum class LightType { Directional, Point, Spot };

class LightPrim : public Xform {
public:
    LightPrim(const SdfPath& path, const std::string& name);
    ~LightPrim() override = default;

    PrimType getType() const override { return PrimType::Xform; } // no dedicated PrimType yet

    LightType getLightType() const { return lightType_; }
    void setLightType(LightType t) { lightType_ = t; }

    const Vec3& getColor() const { return color_; }
    void setColor(const Vec3& c) { color_ = c; }

    float getIntensity() const { return intensity_; }
    void setIntensity(float i) { intensity_ = i; }

    // Directional: direction is -Z in local space (use rotation to orient).
    // Convenience: explicit direction override (world-space).
    const Vec3& getDirection() const { return direction_; }
    void setDirection(const Vec3& d) { direction_ = d; }

    // Point / Spot: attenuation range (0 = infinite)
    float getRange() const { return range_; }
    void setRange(float r) { range_ = r; }

    // Spot: cone angles in radians
    float getInnerConeAngle() const { return innerConeAngle_; }
    void setInnerConeAngle(float a) { innerConeAngle_ = a; }

    float getOuterConeAngle() const { return outerConeAngle_; }
    void setOuterConeAngle(float a) { outerConeAngle_ = a; }

private:
    LightType lightType_ = LightType::Directional;
    Vec3 color_{1.0f, 1.0f, 1.0f};
    float intensity_ = 1.0f;
    Vec3 direction_{0.0f, -1.0f, 0.0f};
    float range_ = 0.0f;
    float innerConeAngle_ = 0.0f;
    float outerConeAngle_ = 0.7854f; // ~45 degrees
};

} // namespace light3d
