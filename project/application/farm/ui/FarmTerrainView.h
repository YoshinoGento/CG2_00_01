#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmGrowthSystem.h"

namespace farmui {
struct TerrainViewState {
    bool preview = false;
    bool canConfirm = false;
    bool canRaise = false, canLower = false, canCanal = false, canSource = false;
    bool canPath = false, canUndo = false, canRedo = false, canCompost = false;
    bool canSetIrrigation = false, irrigationEnabled = true;
    Label status = Label::Terrain;
    std::string tileValues;
    std::size_t changeCount = 0;
    farm::FarmCanalPathIssue pathIssue = farm::FarmCanalPathIssue::None;
    int blockedTileIndex = -1;
};

// Availability comes from Systems; the view emits requests and never edits tiles.
inline void BuildTerrainView(View& view, const TerrainViewState& state) {
    view.terrain = true;
    const bool rejectedExtension = state.preview &&
        state.pathIssue != farm::FarmCanalPathIssue::None;
    if (state.preview && !state.canConfirm) {
        view.Add(Label::PreviewStale, {26, 24, 610, 38});
    } else if (state.preview && state.pathIssue == farm::FarmCanalPathIssue::BlockedTile) {
        view.Metric(Label::PathBlocked, {26, 24, 610, 38}, 400,
            state.blockedTileIndex >= 0 ? "#" + std::to_string(state.blockedTileIndex) : "--");
    } else {
        view.Add(rejectedExtension ? Label::PathNonStraight : state.status, {26, 24, 610, 38});
    }
    const bool intakeAvailable = !state.preview && state.canSetIrrigation;
    view.Add(Label::IntakeOn, {658, 24, 122, 40}, {Action::SetIrrigation, 1},
        intakeAvailable, state.canSetIrrigation && state.irrigationEnabled);
    view.Add(Label::IntakeOff, {786, 24, 122, 40}, {Action::SetIrrigation, 0},
        intakeAvailable, state.canSetIrrigation && !state.irrigationEnabled);
    view.Add(Label::Resume, {950, 24, 298, 40}, {Action::TerrainExit});
    view.Metric(Label::Tile, {26, 68, 910, 36}, 510, state.tileValues);
    view.Metric(Label::Changed, {950, 68, 298, 36}, 180, std::to_string(state.changeCount));
    int index = 0;
    const auto button = [&](Label label, Action action, bool enabled, bool selected = false) {
        const int i = index++;
        view.Add(label, {28.0f + (i % 4)*310, 532.0f + (i / 4)*56, 296, 44},
            {action}, enabled, selected);
    };
    button(Label::Raise, Action::Raise, !state.preview && state.canRaise);
    button(Label::Lower, Action::Lower, !state.preview && state.canLower);
    button(Label::Canal, Action::Canal, !state.preview && state.canCanal);
    button(Label::Source, Action::Source, !state.preview && state.canSource);
    button(Label::Path, Action::Path, !state.preview && state.canPath);
    button(Label::RemovePath, Action::RemovePath, !state.preview && state.canPath);
    button(Label::Undo, Action::Undo, !state.preview && state.canUndo);
    button(Label::Redo, Action::Redo, !state.preview && state.canRedo);
    button(rejectedExtension && state.canConfirm ? Label::ConfirmCandidates : Label::Confirm,
        Action::Confirm, state.preview && state.canConfirm, state.preview);
    button(Label::Cancel, Action::Cancel, state.preview);
    button(Label::Compost, Action::Compost, !state.preview && state.canCompost);
    button(Label::Overview, Action::Overview, true);
}

inline void BuildPlayQuickView(View& view, bool paused, bool canEdit, bool canControlTime = true,
    float timeScale = 1.0f) {
    view.Add(Label::Terrain, {350, 24, 178, 44}, {Action::TerrainField}, canEdit);
    view.Add(!canControlTime ? Label::Paused : paused ? Label::Play : Label::Pause,
        {536, 24, 146, 44}, {Action::Pause}, canControlTime);
    constexpr std::array labels{Label::SpeedCompact1, Label::SpeedCompact2, Label::SpeedCompact4};
    constexpr std::array speeds{1, 2, 4};
    for (std::size_t i = 0; i < speeds.size(); ++i)
        view.Add(labels[i], {690.0f + static_cast<float>(i) * 60.0f, 24, 56, 44},
            {Action::Speed, speeds[i]}, canControlTime,
            canControlTime && !paused && timeScale == static_cast<float>(speeds[i]));
}

inline void BuildWaterGuidanceView(View& view, const FarmWaterGuidance& state, bool canEdit) {
    if (!state.visible) return;
    view.waterGuidance = true;
    Label supply = Label::SupplyNone;
    if (state.intakeClosed) supply = Label::IntakeClosed;
    else switch (state.supply) {
    case farm::FarmWaterStatus::Available: supply = Label::SupplyAvailable; break;
    case farm::FarmWaterStatus::Retained: supply = Label::SupplyRetained; break;
    case farm::FarmWaterStatus::Waiting: supply = Label::SupplyWaiting; break;
    case farm::FarmWaterStatus::Dry: supply = Label::SupplyDry; break;
    default: break;
    }
    Label advice = Label::WaterAdviceUnknown;
    switch (state.advice) {
    case FarmWaterAdvice::Till: advice = Label::WaterAdviceTill; break;
    case FarmWaterAdvice::Plant: advice = Label::WaterAdvicePlant; break;
    case FarmWaterAdvice::Harvest: advice = Label::WaterAdviceHarvest; break;
    case FarmWaterAdvice::Water: advice = Label::WaterAdviceWater; break;
    case FarmWaterAdvice::CheckSupply: advice = Label::WaterAdviceSupply; break;
    case FarmWaterAdvice::CloseIntake: advice = Label::WaterAdviceClose; break;
    case FarmWaterAdvice::AvoidWater: advice = Label::WaterAdviceAvoid; break;
    case FarmWaterAdvice::Monitor: advice = Label::WaterAdviceMonitor; break;
    default: break;
    }
    view.Add(supply, {390, 448, 628, 38}, {Action::TerrainField}, canEdit);
    view.Add(advice, {390, 496, 628, 38});
}
}
