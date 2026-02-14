#include "light3d/timeline.h"
#include "light3d/stage.h"
#include "light3d/xform.h"
#include <algorithm>
#include <cassert>

namespace light3d {

// --- Track<T> implementation ---

// Interpolation helpers
namespace detail {

inline float interpolate(float a, float b, float t) { return a + (b - a) * t; }
inline Vec3 interpolate(const Vec3& a, const Vec3& b, float t) { return lerp(a, b, t); }
inline Quat interpolate(const Quat& a, const Quat& b, float t) { return slerp(a, b, t); }

} // namespace detail

template <typename T>
void Track<T>::addKeyframe(float time, const T& value) {
    Keyframe<T> kf{time, value};
    auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), kf,
        [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.time < b.time; });
    keyframes_.insert(it, kf);
}

template <typename T>
T Track<T>::sample(float time) const {
    if (keyframes_.empty()) return T{};
    if (keyframes_.size() == 1 || time <= keyframes_.front().time) return keyframes_.front().value;
    if (time >= keyframes_.back().time) return keyframes_.back().value;

    // Find bracket
    Keyframe<T> dummy{time, T{}};
    auto it = std::upper_bound(keyframes_.begin(), keyframes_.end(), dummy,
        [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.time < b.time; });
    auto hi = it;
    auto lo = hi - 1;

    float span = hi->time - lo->time;
    float frac = (span > 1e-8f) ? (time - lo->time) / span : 0.0f;
    return detail::interpolate(lo->value, hi->value, frac);
}

// Explicit instantiations
template class Track<float>;
template class Track<Vec3>;
template class Track<Quat>;

// --- AnimationClip ---

AnimationClip::AnimationClip(const std::string& name)
    : name_(name) {}

template <typename T>
Track<T>& AnimationClip::addTrack(const SdfPath& targetPath, const std::string& propertyName, PropertyType type) {
    auto* track = new Track<T>();
    TrackBinding binding;
    binding.targetPath = targetPath;
    binding.propertyName = propertyName;
    binding.type = type;
    binding.trackData = std::unique_ptr<void, void(*)(void*)>(
        track, [](void* p) { delete static_cast<Track<float>*>(p); }
    );
    // We need type-correct deleter, but since all Track<T> are standard-layout-ish
    // with just a vector, this works. For correctness, use per-type deleter:
    if (type == PropertyType::Vec3) {
        binding.trackData = std::unique_ptr<void, void(*)(void*)>(
            track, [](void* p) { delete static_cast<Track<Vec3>*>(p); }
        );
    } else if (type == PropertyType::Quat) {
        binding.trackData = std::unique_ptr<void, void(*)(void*)>(
            track, [](void* p) { delete static_cast<Track<Quat>*>(p); }
        );
    } else {
        binding.trackData = std::unique_ptr<void, void(*)(void*)>(
            track, [](void* p) { delete static_cast<Track<float>*>(p); }
        );
    }

    bindings_.push_back(std::move(binding));
    return *track;
}

// Explicit template instantiations for addTrack
template Track<float>& AnimationClip::addTrack<float>(const SdfPath&, const std::string&, PropertyType);
template Track<Vec3>& AnimationClip::addTrack<Vec3>(const SdfPath&, const std::string&, PropertyType);
template Track<Quat>& AnimationClip::addTrack<Quat>(const SdfPath&, const std::string&, PropertyType);

void AnimationClip::apply(Stage& stage, float time) const {
    for (const auto& binding : bindings_) {
        Prim* prim = stage.getPrimAtPath(binding.targetPath);
        if (!prim) continue;

        auto* xform = dynamic_cast<Xform*>(prim);
        if (!xform) continue;

        // Match property name to Xform fields
        if (binding.propertyName == "translation" && binding.type == PropertyType::Vec3) {
            auto* track = static_cast<Track<Vec3>*>(binding.trackData.get());
            xform->setTranslation(track->sample(time));
        } else if (binding.propertyName == "rotation" && binding.type == PropertyType::Quat) {
            auto* track = static_cast<Track<Quat>*>(binding.trackData.get());
            xform->setRotation(track->sample(time));
        } else if (binding.propertyName == "scale" && binding.type == PropertyType::Vec3) {
            auto* track = static_cast<Track<Vec3>*>(binding.trackData.get());
            xform->setScale(track->sample(time));
        }
    }
}

// --- Timeline ---

AnimationClip& Timeline::addClip(const std::string& name) {
    clips_.push_back(std::make_unique<AnimationClip>(name));
    return *clips_.back();
}

AnimationClip* Timeline::getClip(const std::string& name) {
    for (auto& clip : clips_) {
        if (clip->getName() == name) return clip.get();
    }
    return nullptr;
}

void Timeline::advance(float deltaTime) {
    if (!playing_) return;
    currentTime_ += deltaTime;

    // Find max duration across all clips for looping
    if (looping_) {
        float maxDuration = 0.0f;
        for (auto& clip : clips_) {
            if (clip->duration() > maxDuration) maxDuration = clip->duration();
        }
        if (maxDuration > 0.0f && currentTime_ > maxDuration) {
            currentTime_ = std::fmod(currentTime_, maxDuration);
        }
    }
}

void Timeline::evaluate(Stage& stage) {
    for (auto& clip : clips_) {
        clip->apply(stage, currentTime_);
    }
}

} // namespace light3d
