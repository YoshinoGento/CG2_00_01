#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmOverviewCamera.h"
#include "farm/system/FarmArrivalPresentation.h"
#include <array>
#include <span>

// A value snapshot: no Grid, Camera, UI or GPU resource survives through this object.
class FarmStartPresentationSystem final {
public:
    static constexpr std::size_t kMaximumCells = 512;
    static constexpr std::size_t kMaximumRoute = 64;
    static constexpr float kApproachEnd = 1.35f;
    static constexpr float kSourceEnd = 2.10f;
    static constexpr float kRouteEnd = 4.30f;
    static constexpr float kReturnEnd = 5.90f;
    static constexpr float kDuration = 6.50f;
    static constexpr float kSkipSeconds = 0.35f;
    static constexpr float kOverviewOnlySeconds = 3.0f;
    // The arrival props and camera are authored around this initial field footprint.
    static constexpr int kArrivalGridWidth = 5;
    static constexpr int kArrivalGridHeight = 4;
    enum class Subject { Farm, Source, Canal };
    struct Cell {
        farm::OverviewPose pose{};
        int upstream = -1;
        bool source = false;
        bool canal = false;
    };
    struct Frame {
        farm::OverviewPose pose{};
        Subject subject = Subject::Farm;
        float bars = 1.0f;
        bool showGoal = false;
        bool canSkip = false;
        bool arrival = false;
        float arrivalTime = 0, cover = 0;
    };

    [[nodiscard]] static bool SupportsArrival(const farm::FarmGrid& grid) noexcept {
        if (grid.GetWidth() != kArrivalGridWidth || grid.GetHeight() != kArrivalGridHeight ||
            grid.GetTileCount() != kArrivalGridWidth * kArrivalGridHeight) return false;
        // Clock/load history does not describe the field. No Grid reference is retained.
        for (int i = 0; i < grid.GetTileCount(); ++i) {
            const auto* tile = grid.GetTile(i);
            if (!tile || tile->feature != farm::FarmTileFeature::None ||
                tile->state != farm::FarmTileState::Empty || tile->crop != farm::CropType::None ||
                tile->heightLevel != 0 || tile->moisture != 0.0f || tile->growth != 0.0f) return false;
        }
        return true;
    }

    bool Begin(std::span<const Cell> cells, int width,
        farm::OverviewPose initial, farm::OverviewPose final) noexcept {
        *this = {};
        if (!Finite(initial) || !Finite(final)) return false;
        initial_ = initial; final_ = final; skipFrom_ = initial;
        // The longest bounded upstream chain must end at a real source. Invalid
        // links/cycles/oversized maps fall back to the farm, never a fictitious route.
        if (width > 0 && !cells.empty() && cells.size() <= kMaximumCells &&
            cells.size() % static_cast<std::size_t>(width) == 0) {
            for (std::size_t end = 0; end < cells.size(); ++end) {
                std::array<int, kMaximumRoute> reverse{};
                std::size_t count = 0;
                int index = static_cast<int>(end);
                while (index >= 0 && static_cast<std::size_t>(index) < cells.size() && count < reverse.size()) {
                    const auto& cell = cells[static_cast<std::size_t>(index)];
                    if (!Finite(cell.pose) || (!cell.source && !cell.canal)) break;
                    bool repeated = false;
                    for (std::size_t i = 0; i < count; ++i) repeated |= reverse[i] == index;
                    if (repeated) break;
                    reverse[count++] = index;
                    if (cell.source) {
                        if (count > routeCount_) {
                            routeCount_ = count;
                            for (std::size_t i = 0; i < count; ++i)
                                route_[i] = cells[static_cast<std::size_t>(reverse[count - 1 - i])].pose;
                        }
                        break;
                    }
                    const int next = cell.upstream;
                    if (next < 0 || static_cast<std::size_t>(next) >= cells.size() ||
                        std::abs(next % width - index % width) + std::abs(next / width - index / width) != 1) break;
                    index = next;
                }
            }
        }
        active_ = true;
        return true;
    }

    bool BeginArrival(farm::OverviewPose final) noexcept {
        *this = {};
        if (!Finite(final)) return false;
        final_ = final;
        arrival_ = active_ = true;
        return true;
    }

    void Update(float seconds, bool anyHeld, bool skipTriggered) noexcept {
        if (!active_) return;
        if (!anyHeld) armed_ = true;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 0.10f) : 0.0f;
        if (skipTriggered && armed_ && !skipping_) {
            skipFrom_ = GetFrame().pose;
            skipping_ = true;
        }
        if (skipping_) skipTime_ = (std::min)(kSkipSeconds, skipTime_ + dt);
        else elapsed_ = (std::min)(Duration(), elapsed_ + dt);
        // The frame completing this gate is still consumed by RuntimeController.
        if ((skipping_ ? skipTime_ >= kSkipSeconds : elapsed_ >= Duration()) && !anyHeld) {
            active_ = false;
            completed_ = true;
        }
    }
    [[nodiscard]] Frame GetFrame() const noexcept {
        Frame frame;
        frame.arrival = arrival_;
        frame.arrivalTime = elapsed_;
        frame.canSkip = active_ && armed_ && !skipping_;
        if (skipping_) {
            const float t = Smooth(skipTime_ / kSkipSeconds);
            frame.pose = Blend(skipFrom_, final_, t);
            frame.bars = 1.0f - t;
            frame.showGoal = false;
            frame.cover = 0;
            return frame;
        }
        if (arrival_) {
            const auto arrival = farm::arrival::Sample(elapsed_,final_);
            frame.pose=arrival.pose; frame.bars=arrival.bars;
            frame.cover=arrival.cover; frame.showGoal=arrival.showDay;
            return frame;
        }
        frame.showGoal = elapsed_ >= kRouteEnd || routeCount_ == 0;
        if (routeCount_ == 0) {
            constexpr float kSettleSeconds = 0.6f;
            const float settleAt = kOverviewOnlySeconds - kSettleSeconds;
            frame.pose = Blend(initial_, final_, Smooth(elapsed_ / settleAt));
            frame.bars = 1.0f - Smooth((elapsed_ - settleAt) / kSettleSeconds);
            return frame;
        }
        frame.bars = 1.0f - Smooth((elapsed_ - kReturnEnd) / (kDuration - kReturnEnd));
        const auto first = routeCount_ ? route_[0] : final_;
        const auto last = routeCount_ ? route_[routeCount_ - 1] : final_;
        if (elapsed_ < kApproachEnd) frame.pose = Blend(initial_, first, Smooth(elapsed_ / kApproachEnd));
        else if (elapsed_ < kSourceEnd) frame.pose = first;
        else if (elapsed_ < kRouteEnd) frame.pose = RoutePose(Smooth((elapsed_ - kSourceEnd) / (kRouteEnd - kSourceEnd)));
        else frame.pose = Blend(last, final_, Smooth((elapsed_ - kRouteEnd) / (kReturnEnd - kRouteEnd)));
        if (routeCount_ && elapsed_ < kRouteEnd)
            frame.subject = elapsed_ < kSourceEnd || routeCount_ == 1 ? Subject::Source : Subject::Canal;
        return frame;
    }
    [[nodiscard]] bool ConsumeCompleted() noexcept {
        const bool result = completed_; completed_ = false; return result;
    }
    [[nodiscard]] std::size_t RouteCount() const noexcept { return routeCount_; }
    [[nodiscard]] bool Active() const noexcept { return active_; }
private:
    float Duration() const noexcept { return arrival_ ? farm::arrival::kDuration : routeCount_ ? kDuration : kOverviewOnlySeconds; }
    static bool Finite(farm::OverviewPose pose) noexcept {
        const auto finite = [](Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
        return finite(pose.position) && finite(pose.rotation);
    }
    static float Smooth(float t) noexcept { t = std::clamp(t, 0.0f, 1.0f); return t*t*(3.0f - 2.0f*t); }
    static farm::OverviewPose Blend(farm::OverviewPose a, farm::OverviewPose b, float t) noexcept {
        if (t<=0) return a;
        if (t>=1) return b;
        const auto lerp = [t](Vector3 x, Vector3 y) {
            return Vector3{x.x+(y.x-x.x)*t, x.y+(y.y-x.y)*t, x.z+(y.z-x.z)*t};
        };
        return {lerp(a.position, b.position), lerp(a.rotation, b.rotation)};
    }
    farm::OverviewPose RoutePose(float t) const noexcept {
        if (routeCount_ == 0) return final_;
        if (routeCount_ == 1) return route_[0];
        const float segment = t * static_cast<float>(routeCount_ - 1);
        const std::size_t index = (std::min)(static_cast<std::size_t>(segment), routeCount_ - 2);
        return Blend(route_[index], route_[index + 1], segment - static_cast<float>(index));
    }
    std::array<farm::OverviewPose, kMaximumRoute> route_{};
    std::size_t routeCount_ = 0;
    farm::OverviewPose initial_{}, final_{}, skipFrom_{};
    float elapsed_ = 0, skipTime_ = 0;
    bool active_ = false, armed_ = false, skipping_ = false, completed_ = false;
    bool arrival_ = false;
};
