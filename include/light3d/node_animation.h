#pragma once

#include "timeline.h"  // Track<T>, Keyframe<T>
#include <vector>
#include <string>

namespace light3d {

class Stage;

// Sampled TRS result for a node at a given time
struct TransformSample {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quat rotation = Quat::identity();
    Vec3 scale{1.0f, 1.0f, 1.0f};

    Mat4 toMatrix() const;
};

// Per-node translation/rotation/scale tracks grouped together
struct NodeTransformTrack {
    SdfPath targetPath;
    Track<Vec3> translation;
    Track<Quat> rotation;
    Track<Vec3> scale;

    // Sample all three channels at the given time
    TransformSample sample(float time) const;

    float startTime() const;
    float endTime() const;
    bool empty() const;
};

// Animation clip specifically for node transform animation.
// Groups T/R/S tracks per target node for convenient keyframing.
class NodeAnimationClip {
public:
    explicit NodeAnimationClip(const std::string& name);

    const std::string& getName() const { return name_; }

    // Get or create a transform track for a target node path
    NodeTransformTrack& track(const SdfPath& targetPath);
    const NodeTransformTrack* findTrack(const SdfPath& targetPath) const;
    const std::vector<NodeTransformTrack>& tracks() const { return tracks_; }
    size_t trackCount() const { return tracks_.size(); }

    // Convenience: add individual channel keyframes
    void addTranslationKey(const SdfPath& target, float time, const Vec3& value);
    void addRotationKey(const SdfPath& target, float time, const Quat& value);
    void addScaleKey(const SdfPath& target, float time, const Vec3& value);

    // Add T+R+S at the same time for a node
    void addTransformKey(const SdfPath& target, float time,
                         const Vec3& translation, const Quat& rotation,
                         const Vec3& scale);

    // Evaluate: apply sampled transforms to matching Xform prims on the stage
    void evaluate(Stage& stage, float time) const;

    // Evaluate + recompute world transforms in one call
    void evaluateAndUpdateWorldTransforms(Stage& stage, float time) const;

    // Duration management
    float getDuration() const { return duration_; }
    void setDuration(float d) { duration_ = d; }

    // Compute duration from the maximum track end time
    float computeDurationFromTracks() const;

private:
    std::string name_;
    std::vector<NodeTransformTrack> tracks_;
    float duration_ = 0.0f;
};

} // namespace light3d
