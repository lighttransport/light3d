#include "light3d/node_animation.h"
#include "light3d/stage.h"
#include "light3d/xform.h"
#include <algorithm>
#include <cmath>

namespace light3d {

// --- TransformSample ---

Mat4 TransformSample::toMatrix() const {
    return Mat4::trs(translation, rotation, scale);
}

// --- NodeTransformTrack ---

TransformSample NodeTransformTrack::sample(float time) const {
    TransformSample s;
    if (!translation.empty()) s.translation = translation.sample(time);
    if (!rotation.empty())    s.rotation = rotation.sample(time);
    if (!scale.empty())       s.scale = scale.sample(time);
    return s;
}

float NodeTransformTrack::startTime() const {
    float t = std::numeric_limits<float>::max();
    if (!translation.empty()) t = std::min(t, translation.startTime());
    if (!rotation.empty())    t = std::min(t, rotation.startTime());
    if (!scale.empty())       t = std::min(t, scale.startTime());
    return (t == std::numeric_limits<float>::max()) ? 0.0f : t;
}

float NodeTransformTrack::endTime() const {
    float t = 0.0f;
    if (!translation.empty()) t = std::max(t, translation.endTime());
    if (!rotation.empty())    t = std::max(t, rotation.endTime());
    if (!scale.empty())       t = std::max(t, scale.endTime());
    return t;
}

bool NodeTransformTrack::empty() const {
    return translation.empty() && rotation.empty() && scale.empty();
}

// --- NodeAnimationClip ---

NodeAnimationClip::NodeAnimationClip(const std::string& name)
    : name_(name) {}

NodeTransformTrack& NodeAnimationClip::track(const SdfPath& targetPath) {
    for (auto& t : tracks_) {
        if (t.targetPath == targetPath) return t;
    }
    tracks_.push_back({});
    tracks_.back().targetPath = targetPath;
    return tracks_.back();
}

const NodeTransformTrack* NodeAnimationClip::findTrack(const SdfPath& targetPath) const {
    for (const auto& t : tracks_) {
        if (t.targetPath == targetPath) return &t;
    }
    return nullptr;
}

void NodeAnimationClip::addTranslationKey(const SdfPath& target, float time, const Vec3& value) {
    track(target).translation.addKeyframe(time, value);
}

void NodeAnimationClip::addRotationKey(const SdfPath& target, float time, const Quat& value) {
    track(target).rotation.addKeyframe(time, value);
}

void NodeAnimationClip::addScaleKey(const SdfPath& target, float time, const Vec3& value) {
    track(target).scale.addKeyframe(time, value);
}

void NodeAnimationClip::addTransformKey(
    const SdfPath& target, float time,
    const Vec3& translation, const Quat& rotation, const Vec3& scale) {
    auto& t = track(target);
    t.translation.addKeyframe(time, translation);
    t.rotation.addKeyframe(time, rotation);
    t.scale.addKeyframe(time, scale);
}

void NodeAnimationClip::evaluate(Stage& stage, float time) const {
    for (const auto& trk : tracks_) {
        Prim* prim = stage.getPrimAtPath(trk.targetPath);
        if (!prim) continue;

        auto* xform = dynamic_cast<Xform*>(prim);
        if (!xform) continue;

        TransformSample s = trk.sample(time);

        if (!trk.translation.empty()) xform->setTranslation(s.translation);
        if (!trk.rotation.empty())    xform->setRotation(s.rotation);
        if (!trk.scale.empty())       xform->setScale(s.scale);
    }
}

void NodeAnimationClip::evaluateAndUpdateWorldTransforms(Stage& stage, float time) const {
    evaluate(stage, time);
    stage.updateWorldTransforms();
}

float NodeAnimationClip::computeDurationFromTracks() const {
    float maxEnd = 0.0f;
    for (const auto& trk : tracks_) {
        maxEnd = std::max(maxEnd, trk.endTime());
    }
    return maxEnd;
}

} // namespace light3d
