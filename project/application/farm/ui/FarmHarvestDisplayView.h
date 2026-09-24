#pragma once
#include "farm/ui/FarmQualityView.h"
#include "farm/ui/FarmSeedShopView.h"
#include "farm/system/FarmHarvestDisplaySystem.h"

namespace farmui {
inline void BuildHarvestDisplayView(View& view, const FarmHarvestDisplaySystem::Snapshot& state, bool editable) {
    view.modal = true; view.harvestDisplay = true; view.counterSelection = state.selectedSlot;
    view.Add(Label::HarvestDisplay, {32,24,310,44});
    view.Add(Label::Paused, {350,24,190,44});
    view.Metric(Label::DisplayReserved, {550,24,330,44}, 130, state.reservationId > 0 ? std::to_string(state.reservationId) : "--");
    view.Add(Label::HarvestList, {920,24,300,44}, {Action::HarvestInventory});
    const int count = std::clamp(state.count, 0, FarmHarvestDisplaySystem::kSlots);
    for (int i = 0; i < count; ++i) {
        const auto& record = state.records[i];
        const bool selected = i == state.selectedSlot;
        const auto target = SeedShopLayout::products[i];
        const auto name = CropLabel(record.quality.crop);
        view.harvestFigures[i] = farm::ToCropSlot(record.quality.crop);
        view.Add(name, SeedShopLayout::SelectionTarget(i, true), {Action::HarvestDisplaySelect,record.id,state.generation}, true, selected);
        view.Metric(name, {target.x,454,300,38}, 130, "#"+std::to_string(record.id));
        view.items[view.count-1].darkInk = true;
        view.Metric(Label::DisplayPoints, {target.x,498,300,38}, 156,
            std::to_string(record.quality.score)+"/"+CropSizeText(record.quality.harvestSize));
        view.items[view.count-1].darkInk = true;
    }
    if (count == 0) view.Add(Label::HarvestEmpty, {176,280,640,44});
    const auto* selected = state.selectedSlot >= 0 && state.selectedSlot < count ? &state.records[state.selectedSlot] : nullptr;
    if (selected) {
        const bool reserved = selected->id == state.reservationId;
        view.Add(CropLabel(selected->quality.crop), {880,158,330,38});
        view.Metric(Label::DisplayQuantity, {880,202,330,38}, 174, std::to_string(selected->quantity));
        view.Metric(Label::HarvestDay, {880,246,330,38}, 174, selected->harvestedDay > 0 ? std::to_string(selected->harvestedDay) : "--");
        view.Metric(Label::DisplayEstimate, {880,290,330,38}, 184,
            state.judge.IsValid() ? std::to_string(state.judge.totalPoints)+" / 100" : "--");
        const auto eligibility = !selected->saleProtected ? Label::DisplayProtectFirst : state.entry.issue == FarmContestEntryIssue::Eligible ? Label::DisplayEligible :
            state.entry.issue == FarmContestEntryIssue::OutsidePeriod ? Label::DisplayWrongPeriod :
            state.entry.issue == FarmContestEntryIssue::UnknownHarvestDay ? Label::DisplayUnknownDay :
            state.entry.issue == FarmContestEntryIssue::SeasonEnded ? Label::DisplayNoContest : Label::ContestEligibility;
        view.Add(eligibility, {880,334,338,38});
        view.Add(reserved ? Label::ContestCancel : Label::ContestReserve, {880,386,338,44},
            {reserved ? Action::CancelContestReservation : Action::ReserveContestHarvest,selected->id,state.generation},
            editable && (reserved || state.entry.issue == FarmContestEntryIssue::Eligible), reserved);
        view.Add(selected->saleProtected ? Label::HarvestProtected : Label::HarvestProtect, {880,444,338,44},
            {selected->saleProtected ? Action::UnprotectHarvest : Action::ProtectHarvest,selected->id,state.generation},
            editable && !reserved, selected->saleProtected);
    }
    view.Add(Label::HarvestPrevious, {48,556,220,40}, {Action::HarvestDisplayPage,-1}, state.page > 0);
    view.Metric(Label::HarvestPageNumber, {284,556,264,40}, 124, std::to_string(state.page+1)+" / "+std::to_string(state.pages));
    view.Add(Label::HarvestNext, {568,556,220,40}, {Action::HarvestDisplayPage,1}, state.page+1 < state.pages);
    view.Add(state.unknownCount > 0 ? Label::HarvestLegacyNotice : Label::DisplayArtNote, {48,602,1170,38});
    view.Add(Label::DisplayReview, {48,648,510,40}, {Action::ContestPreview,1}, state.reservationId > 0);
    view.Add(Label::Resume, {928,648,300,40}, {Action::Close});
}
}
