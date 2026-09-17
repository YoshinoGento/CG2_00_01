#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmHUD.h"
#include "farm/system/FarmPlayFlow.h"
#include <cstdio>

namespace farmui {
inline void BuildPlayFlowView(View& view, FarmPlayFlow::Phase phase, const FarmHUDViewData& data) {
    view.modal = true;
    const bool result = phase == FarmPlayFlow::Phase::Result;
    view.Add(data.contestSeason ? Label::SeasonMode : result ? Label::TrialComplete : Label::TrialStart, {166, 66, 940, 52});
    view.Add(data.contestSeason ? Label::SeasonGoal : Label::TrialGoal, {166, 130, 940, 42});
    char text[64]{};
    if (data.contestSeason) std::snprintf(text, sizeof(text), "%d", data.day);
    else std::snprintf(text, sizeof(text), "%dG / %dG", data.money, data.goalMoney);
    view.Value(data.contestSeason ? Label::TrialDay : Label::TrialMoney, 188, text);
    std::snprintf(text, sizeof(text), "%d", data.day);
    if (!data.contestSeason) view.Value(Label::TrialDay, 234, text);
    if (result) {
        view.Add(Label::TrialResultNote, {166, 302, 940, 42});
        std::snprintf(text, sizeof(text), "%d / %dG", data.cropCount, data.saleValue);
        view.Value(Label::TrialRemaining, 354, text);
    } else {
        view.Add(Label::TrialGrow, {166, 302, 940, 42});
        view.Add(Label::TrialWater, {166, 354, 940, 42});
        view.Add(data.contestSeason ? Label::SeasonPrepare : Label::TrialSell, {166, 406, 940, 42});
    }
    view.Add(result ? Label::TrialReview : Label::TrialBegin, {166, 492, 452, 48}, {Action::FlowContinue});
    view.Add(Label::Restart, {638, 492, 452, 48}, {Action::Restart});
    view.Add(Label::Records, {166, 552, 452, 48}, {Action::FlowRecords});
    view.Add(data.contestSeason ? Label::TrialMode : Label::SeasonMode, {638,552,452,48},
        {Action::ChangePlayMode,data.contestSeason ? 0 : 1});
    view.Add(data.contestSeason ? Label::SeasonEndRule : Label::TrialGoalOnly, {166, 616, 940, 38});
}
}
