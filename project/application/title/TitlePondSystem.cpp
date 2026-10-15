#include "title/TitlePondSystem.h"
#include "io/JsonFile.h"
#include "title/TitleCelestialGeometry.h"
#include <algorithm>
#include <cmath>

namespace title {
namespace {
constexpr float kMaximumDeltaSeconds = .25f;
constexpr float kAnimationPeriodSeconds = 60;
constexpr float kImpactMaximumRadius = .78f;
constexpr float kImpactViewportInset = .03f;
constexpr float kImpactMaximumY = .96f;
constexpr int kImpactContractionAttempts = 12;
constexpr float kImpactContraction = .75f;
constexpr float kMinimumFlightPixels = 32;
constexpr float kMinimumFlightHeightUV = .06f;
}
bool TitlePondSystem::Initialize(const std::string& path) {
    ready_ = requested_ = false;
    frame_ = {}; layout_ = {}; path_ = {};
    nlohmann::json json;
    if (!JsonFile::Load(path,json) || !json.is_object()) return false;
    try {
        if (!json.at("version").is_number_integer() || json.at("version").get<int>() != 1) return false;
        const auto center = json.at("center").get<std::array<float,3>>();
        const auto radii = json.at("radii").get<std::array<float,2>>();
        layout_ = {{center[0],center[1],center[2]},{radii[0],radii[1]},
            json.at("shoreWidth").get<float>(),json.at("reflectionLine").get<float>()};
        const auto outfall = json.at("outfall").get<std::array<std::array<float,3>,3>>();
        for (std::size_t i=0; i<outfall.size(); ++i)
            layout_.outfall[i] = {outfall[i][0],outfall[i][1],outfall[i][2]};
        layout_.outfallHalfWidth = json.at("outfallHalfWidth").get<float>();
    } catch (const nlohmann::json::exception&) { return false; }
    for (const float value : {layout_.center.x,layout_.center.y,layout_.center.z,layout_.radii.x,
            layout_.radii.y,layout_.shoreWidth,layout_.reflectionLine})
        if (!std::isfinite(value)) return false;
    if (layout_.radii.x < 1 || layout_.radii.x > 12 || layout_.radii.y < 1 || layout_.radii.y > 8 ||
        layout_.shoreWidth < .1f || layout_.shoreWidth > 1 || layout_.reflectionLine < .45f ||
        layout_.reflectionLine > .75f || layout_.center.y >= kCameraPosition.y) return false;
    if (!std::isfinite(layout_.outfallHalfWidth) || layout_.outfallHalfWidth < .1f || layout_.outfallHalfWidth > .6f) return false;
    for (std::size_t i=0; i<layout_.outfall.size(); ++i) {
        const auto p = layout_.outfall[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            std::abs(p.x)>20 || std::abs(p.y)>5 || std::abs(p.z)>20) return false;
        if (i && (p.y>layout_.outfall[i-1].y || p.z>=layout_.outfall[i-1].z ||
            std::abs(p.x-layout_.outfall[i-1].x)>.0001f)) return false;
    }
    const auto mouth = layout_.outfall.back();
    if (std::abs(mouth.y-layout_.center.y)>.001f ||
        std::hypot((mouth.x-layout_.center.x)/layout_.radii.x,(mouth.z-layout_.center.z)/layout_.radii.y) >= .98f) return false;
    const auto world = MatrixMath::MakeAffineMatrix({1,1,1},kCameraRotation,kCameraPosition);
    const auto projection = MatrixMath::MakePerspectiveFovMatrix(kCameraFovY,kWidth/kHeight,.1f,180);
    viewProjection_ = MatrixMath::Multiply(MatrixMath::Inverse(world),projection);
    // Invert the actual rendered plane-to-screen map, without a second camera-ray approximation.
    std::array<std::array<double,3>,3> clip{};
    constexpr std::array<std::size_t,3> components{0,1,3};
    for (std::size_t row=0; row<3; ++row) {
        const auto column = components[row];
        clip[row] = {layout_.radii.x*viewProjection_.m[0][column],
            layout_.radii.y*viewProjection_.m[2][column],
            layout_.center.x*viewProjection_.m[0][column]+layout_.center.y*viewProjection_.m[1][column]+
                layout_.center.z*viewProjection_.m[2][column]+viewProjection_.m[3][column]};
    }
    std::array<double,9> map{};
    for (std::size_t i=0; i<3; ++i) {
        map[i] = .5*(clip[0][i]+clip[2][i]);
        map[3+i] = .5*(clip[2][i]-clip[1][i]);
        map[6+i] = clip[2][i];
    }
    const auto& a = map;
    const double determinant = a[0]*(a[4]*a[8]-a[5]*a[7])-a[1]*(a[3]*a[8]-a[5]*a[6])+
        a[2]*(a[3]*a[7]-a[4]*a[6]);
    if (!std::isfinite(determinant) || std::abs(determinant)<1e-8 || clip[2][2]<=0) return false;
    const std::array<double,9> adjugate{
        a[4]*a[8]-a[5]*a[7],a[2]*a[7]-a[1]*a[8],a[1]*a[5]-a[2]*a[4],
        a[5]*a[6]-a[3]*a[8],a[0]*a[8]-a[2]*a[6],a[2]*a[3]-a[0]*a[5],
        a[3]*a[7]-a[4]*a[6],a[1]*a[6]-a[0]*a[7],a[0]*a[4]-a[1]*a[3]};
    // A negative homogeneous divisor denotes the visible, camera-facing half-plane in the shaders.
    const double scale = -clip[2][2]/determinant;
    for (std::size_t i=0; i<9; ++i) frame_.screenToPond[i] = static_cast<float>(adjugate[i]*scale);
    for (float value : frame_.screenToPond) if (!std::isfinite(value)) return false;
    frame_.reflectionLine = layout_.reflectionLine;
    frame_.geometry = {layout_.center.x,layout_.center.z,layout_.radii.x,layout_.radii.y};
    frame_.camera = {kCameraPosition.x,kCameraPosition.y-layout_.center.y,kCameraPosition.z,0};
    frame_.inflow = {(mouth.x-layout_.center.x)/layout_.radii.x,
        (mouth.z-layout_.center.z)/layout_.radii.y,kCameraPosition.y-layout_.center.y,0};
    ready_ = true;
    return true;
}
void TitlePondSystem::Update(float delta, const Frame& frame) noexcept {
    if (!ready_) return;
    if (std::isfinite(delta) && delta > 0)
        frame_.seconds = std::fmod(frame_.seconds+(std::min)(delta,kMaximumDeltaSeconds),kAnimationPeriodSeconds);
    frame_.nightAmount = std::clamp(frame.nightAmount,0.0f,1.0f);
    frame_.waterColor = frame.waterColor;
    const auto projection = MatrixMath::MakePerspectiveFovMatrix(kCameraFovY,kWidth/kHeight,.1f,180);
    const auto world = MatrixMath::MakeAffineMatrix({1,1,1},kCameraRotation,kCameraPosition);
    const auto solar = CelestialDirection(frame.sun,projection,world);
    frame_.sunlight = {solar.x,solar.y,solar.z,frame.groundExposure};
}
std::optional<Vector2> TitlePondSystem::MapToPond(Vector2 uv) const noexcept {
    if (!ready_ || !std::isfinite(uv.x) || !std::isfinite(uv.y)) return {};
    const auto& h = frame_.screenToPond;
    const float divisor = h[6]*uv.x+h[7]*uv.y+h[8];
    if (divisor >= -.00001f) return {};
    const Vector2 point{(h[0]*uv.x+h[1]*uv.y+h[2])/divisor,(h[3]*uv.x+h[4]*uv.y+h[5])/divisor};
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return {};
    return point;
}
Vector2 TitlePondSystem::ProjectPondPoint(Vector2 point) const noexcept {
    if (!ready_ || !std::isfinite(point.x) || !std::isfinite(point.y)) return {-1,-1};
    const auto projected = MatrixMath::Transform({layout_.center.x+point.x*layout_.radii.x,
        layout_.center.y,layout_.center.z+point.y*layout_.radii.y},viewProjection_);
    return {(projected.x+1)*.5f,(1-projected.y)*.5f};
}
bool TitlePondSystem::RequestStart(std::optional<Vector2> pointer) noexcept {
    if (!ready_ || requested_) return false;
    Vector2 start{kStartCenter.x/kWidth,kStartCenter.y/kHeight};
    if (pointer) {
        if (!HitTestTitleCanvas(*pointer)) return false;
        start = {pointer->x/kWidth,pointer->y/kHeight};
    }
    Vector2 point{(start.x-.5f)*1.4f,.1f};
    if (auto intersection = MapToPond(start)) point = *intersection;
    const float radius = std::hypot(point.x,point.y);
    if (radius > kImpactMaximumRadius) {
        point.x *= kImpactMaximumRadius/radius; point.y *= kImpactMaximumRadius/radius;
    }
    auto impact = ProjectPondPoint(point);
    // Keep extreme clicks on a visible part of this fixed-camera pond, not beyond the viewport.
    for (int attempt=0; attempt<kImpactContractionAttempts && (impact.x < kImpactViewportInset ||
            impact.x > 1-kImpactViewportInset || impact.y < kImpactViewportInset || impact.y > kImpactMaximumY); ++attempt) {
        point.x *= kImpactContraction; point.y *= kImpactContraction;
        impact = ProjectPondPoint(point);
    }
    if (!std::isfinite(impact.x) || !std::isfinite(impact.y) || impact.x <= 0 || impact.x >= 1 ||
        impact.y <= 0 || impact.y >= 1) return false;
    // A click on water still needs a short visible fall above the water plane.
    if (std::hypot((start.x-impact.x)*kWidth,(start.y-impact.y)*kHeight) < kMinimumFlightPixels)
        start.y = (std::max)(0.0f,impact.y-kMinimumFlightHeightUV);
    path_.enabled = true;
    path_.start = {start.x,start.y}; path_.impact = {impact.x,impact.y};
    path_.screenToPond = frame_.screenToPond;
    requested_ = true;
    return true;
}
}
