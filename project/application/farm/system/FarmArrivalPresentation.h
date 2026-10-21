#pragma once
#include "farm/system/FarmOverviewCamera.h"
#include <algorithm>
#include <cmath>

namespace farm::arrival {
inline constexpr float kDuration = 14.4f;
inline constexpr float kRelease = 2.6f;
inline constexpr float kImpact = 3.25f;
inline constexpr float kLiftBegin = 4.0f;
inline constexpr float kRevealEnd = 7.6f;
inline constexpr float kToolsEnd = 10.5f;
inline constexpr float kSettle = 13.3f;
inline constexpr Vector3 kAnchor{-5.2f, -0.235f, 2.8f};
inline constexpr Vector3 kTools{-4.7f, -0.235f, 7.2f};
inline constexpr Vector3 kTip{0.64f, 0.80f, 0.0f};

inline float Smooth(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    return std::clamp(t*t*t*(t*(t*6-15)+10),0.0f,1.0f);
}
inline Vector3 Mix(Vector3 a, Vector3 b, float t) noexcept {
    return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
}
inline OverviewPose LookAt(Vector3 eye, Vector3 target) noexcept {
    const Vector3 d{target.x-eye.x, target.y-eye.y, target.z-eye.z};
    return {eye, {std::atan2(-d.y, std::hypot(d.x,d.z)), std::atan2(d.x,d.z), 0}};
}
inline OverviewPose MixPose(OverviewPose a, OverviewPose b, float t) noexcept {
    if(t<=0) return a;
    if(t>=1) return b;
    return {Mix(a.position,b.position,t),Mix(a.rotation,b.rotation,t)};
}
inline float LeafAngle(float seconds) noexcept {
    return .025f*std::sin(seconds*1.6f)-.065f*Smooth(seconds/kRelease);
}
inline Vector3 LeafTip(float seconds) noexcept {
    const float a = LeafAngle(seconds);
    return {kAnchor.x+kTip.x*std::cos(a)-kTip.y*std::sin(a),
        kAnchor.y+kTip.x*std::sin(a)+kTip.y*std::cos(a), kAnchor.z};
}
struct Frame {
    OverviewPose pose{};
    Vector3 drop{};
    float leafAngle = 0, dropScale = 0, wetness = 0;
    float bars = 1, cover = 0, ambience = 0;
    bool showDay = false;
};
// Pure staging math. The wet patch is decoration, never a FarmTile moisture edit.
inline Frame Sample(float seconds, OverviewPose final) noexcept {
    const float t = std::isfinite(seconds) ? std::clamp(seconds,0.0f,kDuration) : 0;
    Frame f;
    f.leafAngle = LeafAngle(t);
    f.drop = LeafTip((std::min)(t,kRelease));
    f.drop.y -= .055f;
    const float fall = std::clamp((t-kRelease)/(kImpact-kRelease),0.0f,1.0f);
    f.drop.y += (kAnchor.y+.012f-f.drop.y)*fall*fall;
    f.dropScale = t<kImpact ? .040f+.008f*Smooth(t/kRelease) : 0;
    f.wetness = Smooth((t-kImpact)/.75f);
    f.cover = 1-Smooth(t/.65f);
    f.bars = 1-Smooth((t-kSettle)/(kDuration-kSettle));
    f.ambience = Smooth(t/1.2f)*(1-Smooth((t-kSettle)/(kDuration-kSettle)));
    f.showDay = t>=kToolsEnd;
    const Vector3 target{ kAnchor.x+.32f,kAnchor.y+.43f,kAnchor.z };
    const auto macro = LookAt({kAnchor.x+.20f,kAnchor.y+.95f,kAnchor.z-3.9f},target);
    const auto soil = LookAt({kAnchor.x+.20f,kAnchor.y+.90f,kAnchor.z-3.9f},
        {kAnchor.x+.50f,kAnchor.y+.35f,kAnchor.z});
    const auto reveal = LookAt({-7.6f,4.7f,-3.8f},{0,.3f,8});
    const auto tools = LookAt({-7.2f,2.1f,2.4f},{-3.7f,.3f,7.6f});
    if(t<kLiftBegin) f.pose=MixPose(macro,soil,Smooth((t-kRelease)/(kLiftBegin-kRelease)));
    else if(t<kRevealEnd) f.pose=MixPose(soil,reveal,Smooth((t-kLiftBegin)/(kRevealEnd-kLiftBegin)));
    else if(t<kToolsEnd) f.pose=MixPose(reveal,tools,Smooth((t-kRevealEnd)/(kToolsEnd-kRevealEnd)));
    else f.pose=MixPose(tools,final,Smooth((t-kToolsEnd)/(kSettle-kToolsEnd)));
    return f;
}
}
