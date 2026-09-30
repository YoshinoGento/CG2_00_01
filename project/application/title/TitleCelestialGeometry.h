#pragma once
#include "title/TitlePresentationSystem.h"
#include "math/Matrix.h"
#include <algorithm>
#include <cmath>

namespace title {
struct CelestialLighting {
    Vector3 direction{0, -1, 0};
    float shadowStrength = 0;
    float intensity = 0;
};

inline Vector3 UnitDirection(Vector3 direction, Vector3 fallback) noexcept {
    const float length = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
    if (!std::isfinite(length) || length < .000001f) return fallback;
    return {direction.x/length, direction.y/length, direction.z/length};
}

// Both sky and light use this rotation-only ray; camera translation must not affect it.
inline Vector3 CelestialDirection(const CelestialBodyFrame& body,
    const Matrix4x4& projection, const Matrix4x4& cameraWorld) noexcept {
    const float xScale = projection.m[0][0], yScale = projection.m[1][1];
    if (!std::isfinite(xScale) || !std::isfinite(yScale) || xScale <= 0 || yScale <= 0 ||
        !std::isfinite(body.center.x) || !std::isfinite(body.center.y)) return {0, -1, 0};
    const Vector3 ray{(body.center.x*2-1)/xScale, (1-body.center.y*2)/yScale, 1};
    return UnitDirection({
        ray.x*cameraWorld.m[0][0]+ray.y*cameraWorld.m[1][0]+cameraWorld.m[2][0],
        ray.x*cameraWorld.m[0][1]+ray.y*cameraWorld.m[1][1]+cameraWorld.m[2][1],
        ray.x*cameraWorld.m[0][2]+ray.y*cameraWorld.m[1][2]+cameraWorld.m[2][2]}, {0, -1, 0});
}

inline float ElevationBlend(float elevation, float low, float high) noexcept {
    const float t = std::clamp((elevation-low)/(high-low), 0.0f, 1.0f);
    return t*t*(3-2*t);
}

inline CelestialLighting MakeCelestialLighting(Vector3 sunDirection, Vector3 moonDirection,
    float groundExposure) noexcept {
    sunDirection = UnitDirection(sunDirection, {0, -1, 0});
    moonDirection = UnitDirection(moonDirection, {0, -1, 0});
    constexpr float kFillTransitionElevation = .05f;
    constexpr float kFullShadowElevation = .16f;
    constexpr float kDayShadowStrength = .70f;
    const float solar = ElevationBlend(sunDirection.y, 0, kFillTransitionElevation);
    const float lunar = ElevationBlend(moonDirection.y, 0, kFillTransitionElevation)*(1-solar);
    // Fade via diffuse sky fill at the horizon; whenever shadows exist, rays exactly oppose the sun.
    const Vector3 ray{-sunDirection.x*solar-moonDirection.x*lunar,
        -(1-solar-lunar)-sunDirection.y*solar-moonDirection.y*lunar,
        -sunDirection.z*solar-moonDirection.z*lunar};
    const auto direction = UnitDirection(ray, {0, -1, 0});
    const float exposure = std::isfinite(groundExposure) ? std::clamp(groundExposure,0.0f,1.0f) : 0;
    // Matches Object3D.PS: horizontal unshadowed ground receives ambient .18 + diffuse .82*NdotL.
    // Compensate the changing fill direction so horizon transitions cannot cause brightness surges.
    constexpr float kAmbientResponse = .18f, kDiffuseResponse = .82f;
    const float groundResponse = kAmbientResponse + kDiffuseResponse*(std::max)(-direction.y,0.0f);
    return {direction,
        kDayShadowStrength*ElevationBlend(sunDirection.y, kFillTransitionElevation, kFullShadowElevation),
        exposure/groundResponse};
}
}
