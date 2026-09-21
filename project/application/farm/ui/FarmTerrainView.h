#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"

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

inline void BuildPlayQuickView(View& view, bool paused, bool canEdit, bool canControlTime = true) {
    view.Add(Label::Terrain, {350, 24, 246, 44}, {Action::TerrainField}, canEdit);
    view.Add(!canControlTime ? Label::Paused : paused ? Label::Play : Label::Pause,
        {612, 24, 246, 44}, {Action::Pause}, canControlTime);
}

inline void BuildIntakeStatusView(View& view, bool closed, bool canEdit) {
    if (closed) view.Add(Label::IntakeClosed, {390, 496, 300, 38}, {Action::TerrainField}, canEdit);
}
}
