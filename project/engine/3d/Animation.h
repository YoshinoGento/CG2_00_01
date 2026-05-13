#pragma once
#include "math/Matrix.h"
#include <vector>
#include <string>
#include <map>

// キーフレームのテンプレート構造体 (資料スライド 6)
template <typename tValue>
struct Keyframe {
    float time;   // 秒単位
    tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

// 各ノードのアニメーション (資料スライド 7)
struct NodeAnimation {
    std::vector<KeyframeVector3> translate;
    std::vector<KeyframeQuaternion> rotate;
    std::vector<KeyframeVector3> scale;
};

// アニメーション全体 (資料スライド 8)
struct Animation {
    float duration; // 全体の長さ（秒）
    std::map<std::string, NodeAnimation> nodeAnimations; // ノード名とアニメーションの紐付け
};

// 任意時刻のアニメーション値を計算します
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