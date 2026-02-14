#pragma once

#include "math.h"
#include "prim.h"
#include "property.h"
#include <vector>
#include <string>
#include <memory>

namespace light3d {

class Stage;

// --- Keyframe ---

template <typename T>
struct Keyframe {
    float time;
    T value;
};

// --- Track ---

template <typename T>
class Track {
public:
    void addKeyframe(float time, const T& value);
    T sample(float time) const;
    bool empty() const { return keyframes_.empty(); }
    float startTime() const { return keyframes_.empty() ? 0.0f : keyframes_.front().time; }
    float endTime() const { return keyframes_.empty() ? 0.0f : keyframes_.back().time; }

private:
    std::vector<Keyframe<T>> keyframes_; // sorted by time
};

// --- AnimationClip ---

struct TrackBinding {
    SdfPath targetPath;      // e.g. "/World/Character"
    std::string propertyName; // e.g. "translation"
    PropertyType type;
    std::unique_ptr<void, void(*)(void*)> trackData; // type-erased Track<T>

    TrackBinding() : trackData(nullptr, [](void*){}) {}
};

class AnimationClip {
public:
    explicit AnimationClip(const std::string& name);

    const std::string& getName() const { return name_; }

    template <typename T>
    Track<T>& addTrack(const SdfPath& targetPath, const std::string& propertyName, PropertyType type);

    // Apply sampled values to the stage at the given time
    void apply(Stage& stage, float time) const;

    float duration() const { return duration_; }
    void setDuration(float d) { duration_ = d; }

private:
    std::string name_;
    std::vector<TrackBinding> bindings_;
    float duration_ = 0.0f;
};

// --- Timeline ---

class Timeline {
public:
    Timeline() = default;

    AnimationClip& addClip(const std::string& name);
    AnimationClip* getClip(const std::string& name);

    void setPlaying(bool playing) { playing_ = playing; }
    bool isPlaying() const { return playing_; }

    void setLooping(bool loop) { looping_ = loop; }
    bool isLooping() const { return looping_; }

    void setTime(float t) { currentTime_ = t; }
    float getTime() const { return currentTime_; }

    void advance(float deltaTime);
    void evaluate(Stage& stage);

private:
    std::vector<std::unique_ptr<AnimationClip>> clips_;
    float currentTime_ = 0.0f;
    bool playing_ = false;
    bool looping_ = false;
};

} // namespace light3d
