#pragma once
#include "math/Matrix.h"

#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include <cstddef>

template <typename tValue>
struct Keyframe {
    float time;
    tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

struct NodeAnimation {
    std::vector<KeyframeVector3> translate;
    std::vector<KeyframeQuaternion> rotate;
    std::vector<KeyframeVector3> scale;
};

struct Animation {
    float duration = 0.0f;
    std::map<std::string, NodeAnimation> nodeAnimations;
};

template <typename tValue>
tValue CalculateValue(const std::vector<Keyframe<tValue>>& keyframes, float time) {
    if (keyframes.empty()) {
        return tValue();
    }
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }
    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].time && time < keyframes[i + 1].time) {
            float t = (time - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
            if constexpr (std::is_same_v<tValue, Quaternion>) {
                return MatrixMath::Slerp(keyframes[i].value, keyframes[i + 1].value, t);
            } else {
                return MatrixMath::Lerp(keyframes[i].value, keyframes[i + 1].value, t);
            }
        }
    }
    return keyframes.back().value;
}
