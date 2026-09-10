#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/ui/FarmHUD.h"

namespace farmui {
inline void BuildFieldActionView(View& view, const FarmToolActionResult& evaluation,
    bool paused, bool cleared, FarmHUDNextAction nextAction = FarmHUDNextAction::SelectTile) {
    view.fieldActions = true;
    Label action = Label::ApplyHoe;
    switch (evaluation.tool) {
    case FarmTool::Hoe: action = Label::ApplyHoe; break;
    case FarmTool::Water: action = Label::ApplyWater; break;
    case FarmTool::Seed: action = Label::ApplySeed; break;
    case FarmTool::Harvest: action = Label::ApplyHarvest; break;
    default: break;
    }
    Label reason = Label::ActionReady;
    switch (evaluation.status) {
    case FarmToolActionStatus::Applied:
    case FarmToolActionStatus::Harvested: break;
    case FarmToolActionStatus::InvalidTarget: reason = Label::ActionNoTile; break;
    case FarmToolActionStatus::AlreadyWatered: reason = Label::ActionWaterFull; break;
    case FarmToolActionStatus::NoSeed: reason = Label::ActionNoSeed; break;
    case FarmToolActionStatus::NotReady: reason = Label::ActionGrowing; break;
    default: reason = Label::ActionOtherTool; break;
    }
    if (paused) reason = Label::Paused;
    if (cleared) reason = Label::ClearLocked;
    const bool shop = !paused && !cleared && evaluation.status == FarmToolActionStatus::NoSeed;
    Request request{shop ? Action::OpenSeedShop : Action::ApplyTool};
    bool enabled = !paused && !cleared && (evaluation.Succeeded() || shop);
    if (shop) action = Label::OpenSeedShop;
    if (!paused && !cleared && !evaluation.Succeeded() &&
        (evaluation.status == FarmToolActionStatus::InvalidState || evaluation.status == FarmToolActionStatus::NotReady ||
         evaluation.status == FarmToolActionStatus::AlreadyWatered)) {
        int toolIndex = -1;
        Label switchLabel = Label::ActionOtherTool;
        switch (nextAction) {
        case FarmHUDNextAction::Hoe: toolIndex = 0; switchLabel = Label::SwitchHoe; break;
        case FarmHUDNextAction::WaterOrSeed:
        case FarmHUDNextAction::Water: toolIndex = 1; switchLabel = Label::SwitchWater; break;
        case FarmHUDNextAction::Seed: toolIndex = 2; switchLabel = Label::SwitchSeed; break;
        case FarmHUDNextAction::Harvest: toolIndex = 3; switchLabel = Label::SwitchHarvest; break;
        case FarmHUDNextAction::BuySeed:
            action = Label::OpenSeedShop; reason = Label::ActionNoSeed;
            request = {Action::OpenSeedShop}; enabled = true; break;
        default: break;
        }
        constexpr FarmTool tools[] = {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest};
        if (toolIndex >= 0 && evaluation.tool != tools[toolIndex]) {
            action = switchLabel; reason = Label::NextTool;
            request = {Action::Tool, toolIndex}; enabled = true;
        }
    }
    view.Metric(Label::Selected, {1048, 532, 200, 38}, 106,
        evaluation.tileIndex >= 0 && evaluation.status != FarmToolActionStatus::InvalidTarget
            ? std::to_string(evaluation.tileIndex) : "--");
    view.Add(reason, {1048, 570, 200, 36});
    view.Add(action, {1048, 606, 200, 38}, request, enabled);
}
}
