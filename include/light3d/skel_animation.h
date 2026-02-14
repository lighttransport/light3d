#pragma once

#include "prim.h"
#include "math.h"
#include <vector>

namespace light3d {

class SkelAnimationPrim : public Prim {
public:
    SkelAnimationPrim(const SdfPath& path, const std::string& name);
    ~SkelAnimationPrim() override = default;

    PrimType getType() const override { return PrimType::SkelAnimation; }

    // Set the time samples and joint count
    void setTimeSamples(const std::vector<float>& times, size_t jointCount);

    size_t jointCount() const { return jointCount_; }
    const std::vector<float>& getTimeSamples() const { return times_; }

    // Access per-time-sample, per-joint data
    // Layout: data[timeIndex * jointCount + jointIndex]
    void setTranslation(size_t timeIndex, size_t jointIndex, const Vec3& t);
    void setRotation(size_t timeIndex, size_t jointIndex, const Quat& r);
    void setScale(size_t timeIndex, size_t jointIndex, const Vec3& s);

    // Sample at arbitrary time with interpolation
    struct JointSample {
        Vec3 translation;
        Quat rotation;
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    JointSample sample(float time, size_t jointIndex) const;

private:
    // Find time bracket: returns (lowerIndex, fraction)
    std::pair<size_t, float> findTimeBracket(float time) const;

    std::vector<float> times_;
    size_t jointCount_ = 0;
    std::vector<Vec3> translations_;  // [timeIdx * jointCount_ + jointIdx]
    std::vector<Quat> rotations_;
    std::vector<Vec3> scales_;
};

} // namespace light3d
