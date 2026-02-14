#include "light3d/skel_animation.h"
#include <algorithm>

namespace light3d {

SkelAnimationPrim::SkelAnimationPrim(const SdfPath& path, const std::string& name)
    : Prim(path, name) {}

void SkelAnimationPrim::setTimeSamples(const std::vector<float>& times, size_t jointCount) {
    times_ = times;
    jointCount_ = jointCount;
    size_t total = times.size() * jointCount;
    translations_.resize(total, Vec3{0.0f, 0.0f, 0.0f});
    rotations_.resize(total, Quat::identity());
    scales_.resize(total, Vec3{1.0f, 1.0f, 1.0f});
}

void SkelAnimationPrim::setTranslation(size_t timeIndex, size_t jointIndex, const Vec3& t) {
    translations_[timeIndex * jointCount_ + jointIndex] = t;
}

void SkelAnimationPrim::setRotation(size_t timeIndex, size_t jointIndex, const Quat& r) {
    rotations_[timeIndex * jointCount_ + jointIndex] = r;
}

void SkelAnimationPrim::setScale(size_t timeIndex, size_t jointIndex, const Vec3& s) {
    scales_[timeIndex * jointCount_ + jointIndex] = s;
}

std::pair<size_t, float> SkelAnimationPrim::findTimeBracket(float time) const {
    if (times_.empty()) return {0, 0.0f};
    if (time <= times_.front()) return {0, 0.0f};
    if (time >= times_.back()) return {times_.size() - 1, 0.0f};

    // Binary search for lower bound
    auto it = std::upper_bound(times_.begin(), times_.end(), time);
    size_t hi = static_cast<size_t>(it - times_.begin());
    size_t lo = hi - 1;

    float span = times_[hi] - times_[lo];
    float frac = (span > 1e-8f) ? (time - times_[lo]) / span : 0.0f;
    return {lo, frac};
}

SkelAnimationPrim::JointSample SkelAnimationPrim::sample(float time, size_t jointIndex) const {
    if (times_.empty() || jointIndex >= jointCount_) {
        return {};
    }

    auto [lo, frac] = findTimeBracket(time);

    size_t idxA = lo * jointCount_ + jointIndex;

    if (frac < 1e-6f || lo >= times_.size() - 1) {
        // Exact sample, no interpolation needed
        return {translations_[idxA], rotations_[idxA], scales_[idxA]};
    }

    size_t idxB = (lo + 1) * jointCount_ + jointIndex;

    JointSample result;
    result.translation = lerp(translations_[idxA], translations_[idxB], frac);
    result.rotation = slerp(rotations_[idxA], rotations_[idxB], frac);
    result.scale = lerp(scales_[idxA], scales_[idxB], frac);
    return result;
}

} // namespace light3d
