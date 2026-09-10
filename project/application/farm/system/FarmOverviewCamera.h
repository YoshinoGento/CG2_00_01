#pragma once
#include "math/Struct.h"
#include <algorithm>
#include <array>
#include <cmath>

// Pure camera fitting. Normalized safe areas follow the native 1280x720 HUD layout.
namespace farm {
struct OverviewFrame {
    float left, top, right, bottom;
};
inline constexpr OverviewFrame kFarmOverviewFrame{32.0f/1280, 172.0f/720, 864.0f/1280, 510.0f/720};
inline constexpr OverviewFrame kObservationOverviewFrame{32.0f/1280, 84.0f/720, 1248.0f/1280, 392.0f/720};
inline constexpr OverviewFrame kTerrainOverviewFrame{32.0f/1280, 128.0f/720, 1248.0f/1280, 504.0f/720};
inline constexpr float kOverviewPitch = 0.70f;
struct OverviewPose { Vector3 position{}, rotation{}; };

[[nodiscard]] inline bool FitOverviewCamera(Vector3 minimum, Vector3 maximum,
    float projectionX, float projectionY, float nearClip, float farClip,
    OverviewFrame frame, OverviewPose& output) noexcept {
    const std::array values{minimum.x, minimum.y, minimum.z, maximum.x, maximum.y, maximum.z,
        projectionX, projectionY, nearClip, farClip, frame.left, frame.top, frame.right, frame.bottom};
    for (float value : values) if (!std::isfinite(value)) return false;
    if (minimum.x > maximum.x || minimum.y > maximum.y || minimum.z > maximum.z ||
        projectionX <= 0 || projectionY <= 0 || nearClip <= 0 || farClip <= nearClip ||
        frame.left < 0 || frame.top < 0 || frame.right > 1 || frame.bottom > 1 ||
        frame.right - frame.left < 0.01f || frame.bottom - frame.top < 0.01f) return false;

    const float left = 2 * frame.left - 1, right = 2 * frame.right - 1;
    const float bottom = 1 - 2 * frame.bottom, top = 1 - 2 * frame.top;
    const float targetX = (left + right) * 0.5f, targetY = (bottom + top) * 0.5f;
    const Vector3 center{minimum.x * 0.5f + maximum.x * 0.5f,
        minimum.y * 0.5f + maximum.y * 0.5f, minimum.z * 0.5f + maximum.z * 0.5f};
    const float sine = std::sin(kOverviewPitch), cosine = std::cos(kOverviewPitch);
    std::array<Vector3, 8> corners{};
    float distance = nearClip;
    for (int i = 0; i < 8; ++i) {
        const float x = ((i & 1) ? maximum.x : minimum.x) - center.x;
        const float y = ((i & 2) ? maximum.y : minimum.y) - center.y;
        const float z = ((i & 4) ? maximum.z : minimum.z) - center.z;
        const Vector3 local{x, cosine*y + sine*z, -sine*y + cosine*z};
        if (!std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z)) return false;
        corners[i] = local;
        // Solve each perspective half-space including corner depth, not only a center-plane extent.
        distance = (std::max)(distance, nearClip - local.z);
        distance = (std::max)(distance, (left*local.z - projectionX*local.x) / (targetX-left));
        distance = (std::max)(distance, (projectionX*local.x - right*local.z) / (right-targetX));
        distance = (std::max)(distance, (bottom*local.z - projectionY*local.y) / (targetY-bottom));
        distance = (std::max)(distance, (projectionY*local.y - top*local.z) / (top-targetY));
    }
    distance += 0.05f; // Keep clipping and raster rounding off the safe-area boundary.
    if (!std::isfinite(distance)) return false;
    for (const auto& corner : corners)
        if (!std::isfinite(corner.z) || distance + corner.z >= farClip) return false;
    const float offsetX = targetX * distance / projectionX;
    const float offsetY = targetY * distance / projectionY;
    const OverviewPose pose{{center.x-offsetX, center.y+sine*distance-cosine*offsetY,
        center.z-cosine*distance-sine*offsetY}, {kOverviewPitch, 0, 0}};
    if (!std::isfinite(pose.position.x) || !std::isfinite(pose.position.y) || !std::isfinite(pose.position.z)) return false;
    output = pose;
    return true;
}
}
