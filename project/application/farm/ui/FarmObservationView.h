#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include <cstdio>

namespace farmui {
inline Label ComparisonIssueLabel(FarmComparisonIssue issue) noexcept {
    switch (issue) {
    case FarmComparisonIssue::None: return Label::CompareReady;
    case FarmComparisonIssue::PickTwo: return Label::IssuePickTwo;
    case FarmComparisonIssue::SameTile: return Label::IssueSame;
    case FarmComparisonIssue::NotGrowing: return Label::IssueNotGrowing;
    case FarmComparisonIssue::Crop: return Label::IssueCrop;
    case FarmComparisonIssue::Height: return Label::IssueHeight;
    case FarmComparisonIssue::Growth: return Label::IssueGrowth;
    default: return Label::IssueData;
    }
}
inline Label ComparisonStatusLabel(FarmComparisonStatus status) noexcept {
    switch (status) {
    case FarmComparisonStatus::Running: return Label::Running;
    case FarmComparisonStatus::Stopped: return Label::ObservationStopped;
    case FarmComparisonStatus::Completed: return Label::ObservationCompleted;
    case FarmComparisonStatus::Invalidated: return Label::ObservationInvalid;
    default: return Label::ObservationIdle;
    }
}

// Read-only presentation: tile IDs are validated by the controller before jump requests.
inline void BuildObservationView(View& view, const FarmComparisonView& data,
    const std::array<bool, 2>& canJump, int picking, int selected, bool paused, float speed, bool unlocked) {
    view.observation = true;
    constexpr float titleY = 416, metricY = 458, pickY = 502;
    constexpr float summaryY = 550, transportY = 596, footerY = 644;
    const bool running = data.status == FarmComparisonStatus::Running;
    const bool idle = data.status == FarmComparisonStatus::Idle;
    const Label heading = picking == 0 ? Label::PickingA : picking == 1 ? Label::PickingB : ComparisonStatusLabel(data.status);
    view.Add(heading, {24, 22, 480, 40});
    view.Add(paused ? Label::Paused : Label::Play, {514, 22, 240, 40});
    view.Add(Label::Observe, {770, 22, 210, 40}, {Action::Menu, 3});
    view.Add(Label::ObserveExit, {990, 22, 258, 40}, {Action::ObserveExit});
    char text[64]{};
    for (int slot = 0; slot < 2; ++slot) {
        const auto& row = data.rows[slot];
        const float x = 26.0f + slot * 610;
        std::snprintf(text, sizeof(text), row.tileIndex < 0 ? "--" : "#%d  H%d", row.tileIndex, row.current.heightLevel);
        view.Metric(slot == 0 ? Label::ObservationA : Label::ObservationB, {x, titleY, 302, 38}, 130, text);
        view.Add(slot == 0 ? Label::JumpA : Label::JumpB, {x+312, titleY, 270, 38}, {Action::JumpSlot, slot}, canJump[slot], row.tileIndex >= 0 && selected == row.tileIndex);
        if (row.tileIndex < 0) view.Add(Label::ObservationEmpty, {x, metricY, 220, 38});
        else {
            const auto metric = [&](Label label, float offset, const char* format, double number) {
                std::snprintf(text, sizeof(text), format, number);
                view.Metric(label, {x+offset, metricY, 190, 38}, 92, text);
            };
            metric(Label::Moisture, 0, "%.0f%%", row.current.moisture * 100.0);
            metric(Label::Growth, 190, "%.0f%%", row.current.growth * 100.0);
            metric(Label::ReadySeconds, 380, row.readySeconds < 0 ? "--" : "%.1fs", row.readySeconds);
        }
        view.Add(slot == 0 ? Label::PickA : Label::PickB, {x, pickY, 282, 42}, {Action::PickSlot, slot}, unlocked && !running, picking == slot);
        view.Add(slot == 0 ? Label::PinA : Label::PinB, {x+300, pickY, 282, 42}, {slot == 0 ? Action::PinA : Action::PinB}, unlocked && !running && selected >= 0);
    }
    const Label detail = running ? Label::Running : data.status == FarmComparisonStatus::Invalidated ? Label::ObservationInvalid
        : idle ? ComparisonIssueLabel(data.startIssue) : Label::KeepResults;
    view.Add(detail, {26, summaryY, 680, 38});
    std::snprintf(text, sizeof(text), "%.1fs  x%.0f", data.elapsedSeconds, speed);
    view.Metric(Label::Elapsed, {740, summaryY, 480, 38}, 160, text);
    view.Add(Label::Start, {26, transportY, 222, 42}, {Action::Start}, unlocked && !running && data.startIssue == FarmComparisonIssue::None);
    view.Add(Label::Stop, {260, transportY, 198, 42}, {Action::Stop}, running);
    view.Add(paused ? Label::Play : Label::Pause, {470, transportY, 222, 42}, {Action::Pause});
    view.Add(Label::Speed1, {704, transportY, 170, 42}, {Action::Speed, 1}, unlocked, !paused && speed == 1);
    view.Add(Label::Speed2, {886, transportY, 170, 42}, {Action::Speed, 2}, unlocked, !paused && speed == 2);
    view.Add(Label::Speed4, {1068, transportY, 170, 42}, {Action::Speed, 4}, unlocked, !paused && speed == 4);
    view.Add(Label::Clear, {26, footerY, 310, 36}, {Action::Clear}, !running);
    view.Add(Label::Overview, {348, footerY, 260, 36}, {Action::Overview});
    view.Add(Label::Follow, {620, footerY, 280, 36}, {Action::Follow});
    if (picking >= 0) view.Add(Label::Cancel, {912, footerY, 326, 36}, {Action::PickSlot, -1});
}
}
