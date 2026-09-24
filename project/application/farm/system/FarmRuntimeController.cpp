#include "farm/system/FarmRuntimeController.h"
#include "scene/GamePlayScene.h"
#include "io/Input.h"
#include "farm/system/FarmLayoutSystem.h"
#include "farm/ui/FarmRecordDialog.h"
#include "farm/ui/FarmObservationView.h"
#include "farm/ui/FarmPlayFlowView.h"
#include "farm/ui/FarmFieldActionView.h"
#include "farm/ui/FarmCameraView.h"
#include "farm/ui/FarmQualityView.h"
#include "farm/ui/FarmTerrainView.h"
#include "farm/ui/FarmSeedShopView.h"
#include "farm/ui/FarmHarvestDisplayView.h"
#include "farm/system/FarmOverviewCamera.h"
#include "farm/system/FarmSoilSystem.h"
#include "base/Framework.h"
#include "base/WinApp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <Windows.h>

namespace {
using farmui::Action;
using farmui::Label;
using editor::GamePlayEditorCommandType;
constexpr int kSoilCarePage = 5;
constexpr int kQualityCurrentPage = 6;
constexpr int kQualityHarvestPage = 7;
constexpr int kHarvestInventoryPage = 8;
constexpr int kContestPreviewPage = 9;
constexpr int kContestEntryPage = 10;
constexpr int kContestResultsPage = 11;
constexpr int kHarvestDisplayPage = 12;
template<class... Args> std::string Numbers(const char* format, Args... args) {
    char buffer[96]{};
    std::snprintf(buffer, sizeof(buffer), format, args...);
    return buffer;
}
}

void FarmRuntimeController::BuildView(GamePlayScene& s) {
    view_ = {};
    contestNotice_ = false;
    const auto previousPhase = flow_.GetPhase();
    flow_.Observe(s.farmProgressionSystem_.IsCleared());
    if (s.farmProgressionSystem_.IsContestSeason() && previousPhase != FarmPlayFlow::Phase::Result &&
        flow_.GetPhase() == FarmPlayFlow::Phase::Result) {
        open_ = false; pending_ = Action::None; observation_ = false; terrain_ = false; pickingSlot_ = -1;
    }
    if (flow_.BlocksSimulation() && !open_ && pending_ == Action::None) {
        shop_.Close();
        if (s.farmProgressionSystem_.IsContestSeason() && flow_.GetPhase()==FarmPlayFlow::Phase::Result)
            farmui::BuildSeasonEndView(view_, s.farmEconomySystem_.GetContestResults(),
                FarmContestSeasonSystem::Evaluate(s.farmDateSystem_.GetDay(),s.farmEconomySystem_.GetContestResults()));
        else farmui::BuildPlayFlowView(view_, flow_.GetPhase(), s.BuildFarmHUDViewData());
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    contestNotice_ = s.farmContestDaySystem_.PendingDay() > 0 &&
        !s.farmProgressionSystem_.IsCleared() && pending_ == Action::None;
    if (contestNotice_) {
        shop_.Close();
        farmui::BuildContestDayNoticeView(view_, s.farmContestDaySystem_.PendingDay());
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    if (shop_.IsOpen()) {
        if (s.farmProgressionSystem_.IsCleared() || s.farmIrrigationPreviewSystem_.IsActive()) shop_.Close();
        else { farmui::BuildSeedShopView(view_, shop_, s.farmEconomySystem_); return; }
    }
    view_.modal = open_;
    if (!open_) {
        if (terrain_) {
            const auto& preview = s.farmIrrigationPreviewSystem_;
            const int index = s.farmGrid_.GetSelectedIndex();
            const auto* tile = s.farmGrid_.GetTile(index);
            const bool unlocked = !s.farmProgressionSystem_.IsCleared();
            farmui::TerrainViewState state;
            state.preview = preview.IsActive();
            state.canConfirm = preview.CanConfirm(s.farmGrid_);
            state.canRaise = unlocked && tile && tile->heightLevel < FarmToolActionSystem::kMaximumHeightLevel;
            state.canLower = unlocked && tile && tile->heightLevel > FarmToolActionSystem::kMinimumHeightLevel;
            state.canCanal = unlocked && s.farmToolActionSystem_.CanToggleCanal(s.farmGrid_, index);
            state.canSource = unlocked && s.farmToolActionSystem_.CanToggleWaterSource(s.farmGrid_, index);
            state.canPath = unlocked && tile;
            state.canUndo = unlocked && s.farmToolActionSystem_.GetHistory().CanUndo();
            state.canRedo = unlocked && s.farmToolActionSystem_.GetHistory().CanRedo();
            state.canCompost = unlocked && tile && FarmSoilSystem::CanCompost(*tile);
            state.canSetIrrigation = unlocked && s.farmToolActionSystem_.CanSetIrrigation(s.farmGrid_, index);
            state.irrigationEnabled = tile && tile->irrigationEnabled;
            state.status = state.preview ? (brush_ == 1 ? Label::PathActive : brush_ == 2 ? Label::RemoveActive : Label::Preview)
                : (status_ == Label::Failure ? status_ : Label::PauseCapture);
            state.pathIssue = preview.GetPathIssue();
            if (state.preview && preview.GetOperation() == farm::FarmIrrigationPreviewOperation::RaiseTerrain)
                state.status = Label::RaiseBrush;
            if (state.preview && preview.GetOperation() == farm::FarmIrrigationPreviewOperation::LowerTerrain)
                state.status = Label::LowerBrush;
            state.blockedTileIndex = preview.GetBlockedTileIndex();
            state.changeCount = preview.GetChangeCount();
            // Read candidate values without publishing them to the authoritative grid.
            int displayedIndex = index;
            if (const auto* candidate = preview.GetPreviewGrid()) {
                displayedIndex = candidate->GetSelectedIndex();
                tile = candidate->GetTile(displayedIndex);
            }
            state.tileValues = tile ? Numbers("%d / H%d / %.0f / %.0f", displayedIndex, tile->heightLevel,
                tile->moisture * 100, tile->growth * 100) : "--";
            farmui::BuildTerrainView(view_, state);
            return;
        }
        if (observation_) {
            const auto comparison = s.farmGrowthComparisonSystem_.GetView(s.farmGrid_);
            if (lastComparisonStatus_ == FarmComparisonStatus::Running && comparison.status != FarmComparisonStatus::Running) paused_ = true;
            lastComparisonStatus_ = comparison.status;
            const bool valid = comparison.status != FarmComparisonStatus::Invalidated;
            const std::array<bool, 2> jumps = {valid && s.farmGrid_.GetTile(comparison.rows[0].tileIndex), valid && s.farmGrid_.GetTile(comparison.rows[1].tileIndex)};
            farmui::BuildObservationView(view_, comparison, jumps, pickingSlot_, s.farmGrid_.GetSelectedIndex(),
                paused_, s.farmDateSystem_.GetTimeScale(), !s.farmProgressionSystem_.IsCleared() && !s.farmIrrigationPreviewSystem_.IsActive());
            return;
        }
        const auto hud = s.BuildFarmHUDViewData();
        view_.feedback = hud.feedback != FarmHUDFeedback::None;
        view_.Add(Label::Menu, {1040, 654, 216, 42}, {Action::Menu});
        if (s.farmIrrigationPreviewSystem_.IsActive()) {
            view_.Add(Label::Confirm, {1040, 544, 216, 44}, {Action::Confirm}, s.farmIrrigationPreviewSystem_.CanConfirm(s.farmGrid_));
            view_.Add(Label::Cancel, {1040, 596, 216, 44}, {Action::Cancel});
        } else if (!s.farmCropSelectionSystem_.IsOpen()) {
            farmui::BuildPlayQuickView(view_, paused_, !s.farmProgressionSystem_.IsCleared(),
                !s.farmProgressionSystem_.IsCleared(), s.farmDateSystem_.GetTimeScale());
            const auto* reserved = s.farmEconomySystem_.GetContestReservation();
            const auto* tile = s.farmGrid_.GetSelectedTile();
            const int index = s.farmGrid_.GetSelectedIndex();
            const auto forecast = tile ? s.farmGrowthSystem_.Evaluate(*tile,
                s.farmCropSelectionSystem_.GetSelectedCrop(), s.farmDateSystem_.GetTimeScale(),
                s.farmIrrigationSystem_.GetAvailableIrrigationStrength(s.farmGrid_, index)) : FarmGrowthForecast{};
            farmui::BuildWaterGuidanceView(view_, FarmGrowthSystem::AnalyzeWater(tile, forecast,
                s.farmIrrigationSystem_.GetWaterStatus(s.farmGrid_, index)), !s.farmProgressionSystem_.IsCleared(),
                s.farmToolActionSystem_.CanSetIrrigation(s.farmGrid_, index));
            const bool planted = tile && farm::IsPlantableCrop(tile->crop);
            const auto quality = planted ? s.farmToolActionSystem_.EvaluateHarvestQuality(*tile) : FarmCropQualityResult{};
            farmui::BuildPlayCropStatusView(view_, hud.feedback != FarmHUDFeedback::None,
                s.farmEconomySystem_.GetProtectedCropCount(),
                reserved ? reserved->quality.crop : farm::CropType::None, planted ? &quality.harvestSize : nullptr);
            farmui::BuildCameraView(view_, s.usePlayerCamera_, s.levelGameplay_.HasPlayer());
            view_.Add(Label::ShopTitle, {1040, 376, 216, 44}, {Action::OpenSeedShop}, !s.farmProgressionSystem_.IsCleared());
            view_.Add(Label::HarvestDisplay, {1040, 324, 216, 44}, {Action::HarvestDisplay}, !s.farmProgressionSystem_.IsCleared());
            const auto evaluation = s.farmToolActionSystem_.EvaluateTool(s.farmGrid_,
                s.farmToolSystem_.GetCurrentTool(), s.farmCropSelectionSystem_.GetSelectedCrop(), &s.farmEconomySystem_);
            farmui::BuildFieldActionView(view_, evaluation, paused_, s.farmProgressionSystem_.IsCleared(),
                hud.nextAction, hud.contestSeason);
        }
        return;
    }
    if (page_ == kHarvestDisplayPage && pending_ == Action::None) {
        farmui::BuildHarvestDisplayView(view_, harvestDisplay_.Observe(s.farmEconomySystem_, s.farmDateSystem_.GetDay()),
            !s.farmProgressionSystem_.IsCleared() && !s.farmIrrigationPreviewSystem_.IsActive() && !s.timelineScrubbing_);
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    constexpr Label tabs[] = {Label::Farm, Label::Terrain, Label::Records, Label::Observe, Label::Settings};
    for (int i = 0; i < 5; ++i) view_.Add(tabs[i], {150.0f + i * 196, 52, 188, 48}, {Action::Tab, i}, pending_ == Action::None,
        page_ == i || ((page_ == kSoilCarePage || page_ == kQualityCurrentPage || page_ == kQualityHarvestPage || page_ == kHarvestInventoryPage || page_ == kContestPreviewPage || page_ == kContestEntryPage || page_ == kContestResultsPage) && i == 0));
    view_.Add(Label::Resume, {916, 628, 214, 44}, {Action::Close});
    view_.Add(status_, {166, 628, 726, 42});
    if (pending_ != Action::None) {
        view_.Add(pending_ == Action::ChangePlayMode ? Label::ModeConfirm :
            pending_ == Action::SubmitContest ? Label::ContestSubmitConfirm : Label::ConfirmDestructive, {256, 250, 760, 54});
        view_.Add(pending_ == Action::ChangePlayMode ? Label::ModeAccept :
            pending_ == Action::SubmitContest ? Label::ContestSubmitAccept : Label::Accept, {256, 342, 360, 56}, {Action::Accept});
        view_.Add(Label::Cancel, {648, 342, 360, 56}, {Action::Cancel});
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    int row = 0;
    const auto button = [&](Label label, Action action, int argument = 0, bool enabled = true, bool selected = false) {
        const int i = row++;
        view_.Add(label, {170.0f + (i % 2) * 476, 130.0f + (i / 2) * 57, 450, 48}, {action, argument}, enabled, selected);
    };
    const auto hud = s.BuildFarmHUDViewData();
    const bool unlocked = !s.farmProgressionSystem_.IsCleared() && !s.farmIrrigationPreviewSystem_.IsActive();
    const auto* tile = s.farmGrid_.GetSelectedTile();
    const auto crop = s.farmCropSelectionSystem_.GetSelectedCrop();
    if (page_ == 0) {
        button(Label::Hoe, Action::Tool, 0, unlocked, hud.currentToolIndex == 0);
        button(Label::Water, Action::Tool, 1, unlocked, hud.currentToolIndex == 1);
        button(Label::Seed, Action::Tool, 2, unlocked, hud.currentToolIndex == 2);
        button(Label::Harvest, Action::Tool, 3, unlocked, hud.currentToolIndex == 3);
        button(Label::Tomato, Action::Crop, 2, unlocked, crop == farm::CropType::Tomato);
        button(Label::Carrot, Action::Crop, 1, unlocked, crop == farm::CropType::Carrot);
        button(Label::ShopTitle, Action::OpenSeedShop, 0, unlocked);
        button(Label::Sell, Action::Sell, 0, unlocked && s.farmEconomySystem_.PreviewSale(crop).Succeeded());
        button(Label::SellAll, Action::SellAll, 0, unlocked && s.farmEconomySystem_.PreviewSale().Succeeded());
        button(Label::Pumpkin, Action::Crop, 3, unlocked, crop == farm::CropType::Pumpkin);
        button(Label::SoilCare, Action::SoilCare);
        button(Label::QualityOpen, Action::Quality);
        view_.Value(Label::Money, 482, Numbers("%dG / %dG", hud.money, hud.seedPrice));
        view_.Value(Label::Inventory, 528, Numbers("%d / %d / %dG", hud.seedCount, s.farmEconomySystem_.GetCropCount(crop), s.farmEconomySystem_.PreviewSale(crop).earnedMoney));
        view_.Add(Label::HarvestDisplay, {176,574,920,42}, {Action::HarvestDisplay});
    } else if (page_ == 1) {
        button(Label::Raise, Action::Raise, 0, unlocked && tile && tile->heightLevel < FarmToolActionSystem::kMaximumHeightLevel);
        button(Label::Lower, Action::Lower, 0, unlocked && tile && tile->heightLevel > FarmToolActionSystem::kMinimumHeightLevel);
        button(Label::Canal, Action::Canal, 0, unlocked && s.farmToolActionSystem_.CanToggleCanal(s.farmGrid_, hud.selectedTileIndex));
        button(Label::Source, Action::Source, 0, unlocked && s.farmToolActionSystem_.CanToggleWaterSource(s.farmGrid_, hud.selectedTileIndex));
        button(Label::Path, Action::Path, 0, unlocked && tile);
        button(Label::RemovePath, Action::RemovePath, 0, unlocked && tile);
        button(Label::Undo, Action::Undo, 0, unlocked && s.farmToolActionSystem_.GetHistory().CanUndo());
        button(Label::Redo, Action::Redo, 0, unlocked && s.farmToolActionSystem_.GetHistory().CanRedo());
        button(Label::Confirm, Action::Confirm, 0, s.farmIrrigationPreviewSystem_.CanConfirm(s.farmGrid_));
        button(Label::Cancel, Action::Cancel, 0, s.farmIrrigationPreviewSystem_.IsActive());
        button(Label::Overview, Action::TerrainField, 0, !s.farmProgressionSystem_.IsCleared());
        view_.Value(Label::Tile, 478, Numbers("%d / H%d / %d / %d", hud.selectedTileIndex, hud.selectedTileHeight, hud.selectedTileMoisturePercent, hud.selectedTileGrowthPercent));
        view_.Value(Label::WaterStats, 524, Numbers("%d / %d", s.farmIrrigationSystem_.GetSuppliedCanalCount(), s.farmIrrigationSystem_.GetIrrigationRangeTileCount()));
    } else if (page_ == 2) {
        const auto& docs = s.farmDocumentSystem_.GetDocuments();
        documentIndex_ = std::clamp(documentIndex_, 0, (std::max)(0, static_cast<int>(docs.size()) - 1));
        button(Label::Save, Action::Save, 0, !s.farmIrrigationPreviewSystem_.IsActive() && s.farmDocumentSystem_.FileExists());
        button(Label::SaveCopy, Action::SaveCopy, 0, !s.farmIrrigationPreviewSystem_.IsActive());
        button(Label::Previous, Action::Previous, 0, documentIndex_ > 0);
        button(Label::Next, Action::Next, 0, documentIndex_ + 1 < static_cast<int>(docs.size()));
        button(Label::Load, Action::Load, 0, !docs.empty());
        button(Label::Restart, Action::Restart);
        button(Label::LayoutLibrary, Action::LayoutLibrary, 0, !s.farmIrrigationPreviewSystem_.IsActive());
        view_.Value(Label::Record, 382, Numbers("%d / %zu", docs.empty() ? 0 : documentIndex_ + 1, docs.size()));
        view_.recordNames = {s.farmDocumentSystem_.GetDisplayName(), docs.empty() ? "記録なし" : docs[documentIndex_].displayName};
        view_.Add(docs.empty() ? Label::NoSaves : (s.farmDocumentSystem_.IsDirty() ? Label::Dirty : Label::Clean), {176, 554, 900, 40});
    } else if (page_ == 3) {
        const auto comparison = s.farmGrowthComparisonSystem_.GetView(s.farmGrid_);
        const bool running = comparison.status == FarmComparisonStatus::Running;
        button(Label::PinA, Action::PinA, 0, unlocked && tile && !running);
        button(Label::PinB, Action::PinB, 0, unlocked && tile && !running);
        button(Label::Start, Action::Start, 0, unlocked && !running && comparison.startIssue == FarmComparisonIssue::None);
        button(Label::Stop, Action::Stop, 0, comparison.status == FarmComparisonStatus::Running);
        button(Label::Clear, Action::Clear);
        button(Label::ObserveField, Action::ObserveField, 0, unlocked);
        view_.Value(Label::Compare, 330, Numbers("%d / %d / %.1f", comparison.rows[0].tileIndex, comparison.rows[1].tileIndex, comparison.elapsedSeconds));
        view_.Add(comparison.status == FarmComparisonStatus::Running ? Label::Running : Label::NotRunning, {176, 374, 500, 38});
        if (tile && farm::IsPlantableCrop(tile->crop)) {
            const auto quality = s.farmToolActionSystem_.EvaluateHarvestQuality(*tile);
            view_.Value(Label::Quality, 418, Numbers("%.0f / %.0f / %.0f", quality.maturity * 100, quality.waterBalance * 100, quality.terrainFit * 100));
            view_.Value(Label::Score, 462, Numbers("%d / %dG", quality.score, quality.salePrice));
        } else view_.Add(Label::NoCrop, {176, 418, 850, 38});
        view_.Value(Label::Status, 506, Numbers("A %.0f/%.0f B %.0f/%.0f", comparison.rows[0].current.moisture * 100, comparison.rows[0].current.growth * 100, comparison.rows[1].current.moisture * 100, comparison.rows[1].current.growth * 100));
        if (comparison.startIssue != FarmComparisonIssue::None) view_.Add(farmui::ComparisonIssueLabel(comparison.startIssue), {176, 550, 910, 38});
        if (tile && farm::IsPlantableCrop(tile->crop)) {
            const auto quality = s.farmToolActionSystem_.EvaluateHarvestQuality(*tile);
            view_.Value(Label::NutrientQuality, 590, Numbers("%.0f / 100", quality.nutrientBalance * 100));
        }
    } else if (page_ == kContestPreviewPage) {
        farmui::BuildContestJudgeView(view_, FarmContestJudgeSystem::Evaluate(s.farmEconomySystem_.GetContestReservation()));
    } else if (page_ == kContestEntryPage) {
        farmui::BuildContestEntryView(view_, FarmContestEntrySystem::Evaluate(s.farmDateSystem_.GetDay(),
            s.farmEconomySystem_.GetContestReservation()),
            FarmContestSubmissionSystem::Evaluate(s.farmEconomySystem_,s.farmDateSystem_.GetDay()),
            {Action::SubmitContest,s.farmEconomySystem_.GetContestReservationId(),s.farmEconomySystem_.GetInventoryGeneration()});
    } else if (page_ == kContestResultsPage) {
        farmui::BuildContestResultsView(view_,s.farmEconomySystem_.GetContestResults(),
            FarmContestSeasonSystem::Evaluate(s.farmDateSystem_.GetDay(),s.farmEconomySystem_.GetContestResults()));
    } else if (page_ == kHarvestInventoryPage) {
        const auto& economy = s.farmEconomySystem_;
        farmui::HarvestInventoryViewState state;
        state.records = static_cast<int>(economy.GetHarvestRecordCount());
        state.capacity = static_cast<int>(FarmEconomySystem::kMaxHarvestRecords);
        state.unknownCount = economy.GetUnrecordedCropCount();
        state.protectedCount = economy.GetProtectedCropCount();
        state.inventoryGeneration = economy.GetInventoryGeneration();
        state.canChangeProtection = !s.farmIrrigationPreviewSystem_.IsActive() && !s.timelineScrubbing_;
        state.contestReservationId = economy.GetContestReservationId();
        if (const auto* reserved = economy.GetContestReservation()) state.reservedCrop = reserved->quality.crop;
        state.pages = (std::max)(1, (state.records + state.kRows - 1) / state.kRows);
        state.page = harvestPage_ = std::clamp(harvestPage_, 0, state.pages - 1);
        for (int i = 0; i < state.kRows; ++i) {
            if (const auto* record = economy.GetHarvestRecord(state.page * state.kRows + i))
                state.rows[state.rowCount++] = {record->quality, record->quantity, record->id, record->saleProtected,
                    FarmEconomySystem::CanReserveForContest(*record), record->harvestedDay};
        }
        farmui::BuildHarvestInventoryView(view_, state);
    } else if (page_ == kQualityCurrentPage || page_ == kQualityHarvestPage) {
        const auto quality = page_ == kQualityHarvestPage ? s.farmEconomySystem_.GetLastHarvestQuality()
            : tile ? s.farmToolActionSystem_.EvaluateHarvestQuality(*tile) : FarmCropQualityResult{};
        farmui::BuildQualityView(view_, quality, FarmCropQualitySystem::Analyze(quality),
            hud.selectedTileIndex, page_ == kQualityHarvestPage);
    } else if (page_ == kSoilCarePage) {
        button(Label::Compost, Action::Compost, 0, unlocked && tile && FarmSoilSystem::CanCompost(*tile));
        button(Label::QualityOpen, Action::Quality);
        const auto soilCrop = tile && farm::IsPlantableCrop(tile->crop) ? tile->crop : crop;
        const auto* profile = FarmSoilSystem::Profile(soilCrop);
        const bool soil = tile && tile->feature == farm::FarmTileFeature::None;
        view_.Add(Label::SoilSelectedCrop, {176, 198, 900, 38});
        view_.Add(farmui::CropLabel(soilCrop), {176, 238, 240, 38});
        view_.Metric(Label::Selected, {716, 238, 350, 38}, 120, Numbers("#%d", hud.selectedTileIndex));
        view_.Value(Label::SoilNutrients, 286, soil ? Numbers("%.0f / 100", tile->soilNutrients * 100) : "--");
        view_.Value(Label::NutrientTarget, 332, profile ? Numbers("%.0f / 100", profile->target * 100) : "--");
        view_.Value(Label::NutrientUse, 378, profile ? Numbers("%.0f", profile->consumptionPerGrowth * 100) : "--");
        const auto quality = tile ? s.farmToolActionSystem_.EvaluateHarvestQuality(*tile) : FarmCropQualityResult{};
        view_.Value(Label::NutrientQuality, 424, quality.IsValid() ? Numbers("%.0f / 100", quality.nutrientBalance * 100) : "--");
        const Label advice = !soil || tile->state == farm::FarmTileState::Empty ? Label::SoilTill
            : farm::IsPlantableCrop(tile->crop) ? Label::SoilGrowing
            : tile->soilNutrients >= 1.0f ? Label::SoilFull : Label::SoilReady;
        view_.Add(advice, {176, 484, 920, 38});
        view_.Add(Label::SoilHelp, {176, 534, 920, 38});
        farm::FarmTile preview;
        if (soil) preview = *tile;
        // Read-only species guidance is also available before preparing the soil.
        preview.state = farm::FarmTileState::Tilled;
        preview.feature = farm::FarmTileFeature::None;
        preview.crop = soilCrop;
        const auto water = s.farmGrowthSystem_.Evaluate(preview, soilCrop);
        view_.Value(Label::OptimalWater, 580, Numbers("%.0f - %.0f %%",
            water.goodMoistureMinimum * 100, water.goodMoistureMaximum * 100));
    } else {
        button(paused_ ? Label::Play : Label::Pause, Action::Pause);
        button(Label::Speed1, Action::Speed, 1, unlocked, !paused_ && hud.timeScale == 1);
        button(Label::Speed2, Action::Speed, 2, unlocked, !paused_ && hud.timeScale == 2);
        button(Label::Speed4, Action::Speed, 4, unlocked, !paused_ && hud.timeScale == 4);
        button(Label::Follow, Action::Follow, 0, true, s.usePlayerCamera_);
        button(Label::Overview, Action::Overview, 0, true, !s.usePlayerCamera_);
        button(Label::Exit, Action::Exit);
        button(Label::SeasonMode, Action::ChangePlayMode, 1, true, hud.contestSeason);
        button(Label::TrialMode, Action::ChangePlayMode, 0, true, !hud.contestSeason);
        view_.Value(Label::Day, 440, Numbers("%d / x%.0f", hud.day, hud.timeScale));
        view_.Add(Label::PauseCapture, {176, 492, 900, 40});
    }
    ui_.PrepareNames(view_);
    if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
}

void FarmRuntimeController::Execute(GamePlayScene& s, farmui::Request request) {
    using C = GamePlayEditorCommandType;
    const auto dispatch = [&](C type) {
        editor::GamePlayEditorCommand command{};
        command.type = type;
        command.farmGeneration = s.farmGrid_.GetGeneration();
        command.farmTileIndex = type == C::ConfirmFarmIrrigationPreview ? s.farmIrrigationPreviewSystem_.GetTileIndex() : s.farmGrid_.GetSelectedIndex();
        return s.gamePlayEditorBridge_.Execute(command);
    };
    bool success = false;
    switch (request.action) {
    case Action::ShopSelect:
        shop_.Select(request.argument); return;
    case Action::ShopQuantity:
        shop_.ChangeQuantity(request.argument); return;
    case Action::ShopReview:
        shop_.BeginConfirmation(s.farmEconomySystem_); return;
    case Action::ShopCancel:
        shop_.CancelConfirmation(); return;
    case Action::ShopClose:
        shop_.Close(); return;
    case Action::ShopConfirm: {
        if (s.farmProgressionSystem_.IsCleared() || s.farmIrrigationPreviewSystem_.IsActive()) { shop_.Close(); return; }
        const auto result = shop_.Confirm(s.farmEconomySystem_);
        if (result.Succeeded()) {
            s.farmCropSelectionSystem_.SetSelectedCrop(result.crop);
            s.farmDocumentSystem_.MarkDirty();
            s.farmFeedbackSystem_.ShowSeedPurchased(result.crop, result.purchasedCount, result.spentMoney);
        }
        return;
    }
    case Action::OpenSeedShop:
        if (shop_.IsOpen() || observation_ || terrain_ || pending_ != Action::None || flow_.BlocksSimulation() ||
            s.farmProgressionSystem_.IsCleared() || s.farmIrrigationPreviewSystem_.IsActive() ||
            s.farmCropSelectionSystem_.IsOpen()) return;
        if (!s.farmSeedShopRenderer_.IsReady()) { status_ = Label::Failure; return; }
        shop_.Open(s.farmCropSelectionSystem_.GetSelectedCrop()); open_ = false; focusedItem_ = -1;
        return;
    case Action::ContestDayReview:
    case Action::ContestDayPrepare:
    case Action::ContestDayResume: {
        editor::GamePlayEditorCommand command{};
        command.type = C::AcknowledgeContestDay;
        command.farmGeneration = s.farmGrid_.GetGeneration();
        command.contestDay = request.argument;
        if (!s.gamePlayEditorBridge_.Execute(command)) return;
        if (terrain_) LeaveTerrain(s);
        observation_ = false; pickingSlot_ = -1;
        s.farmCropSelectionSystem_.Cancel();
        pending_ = Action::None;
        paused_ = request.action != Action::ContestDayResume;
        open_ = request.action == Action::ContestDayReview;
        if (open_) page_ = kContestEntryPage;
        status_ = paused_ ? Label::Paused : Label::Ready;
        focusedItem_ = -1;
        return;
    }
    case Action::TerrainField:
        if (!flow_.BlocksSimulation() && !s.farmProgressionSystem_.IsCleared()) EnterTerrain(s);
        return;
    case Action::TerrainExit: LeaveTerrain(s); return;
    case Action::ApplyTool: {
        if (open_ || observation_ || terrain_ || flow_.BlocksSimulation() ||
            s.farmProgressionSystem_.IsCleared() || s.farmIrrigationPreviewSystem_.IsActive() ||
            s.farmCropSelectionSystem_.IsOpen()) return;
        // Revalidate on execution; the displayed evaluation is not mutation authority.
        const auto result = s.farmToolActionSystem_.ApplyToolDetailed(s.farmGrid_,
            s.farmToolSystem_.GetCurrentTool(), s.farmCropSelectionSystem_.GetSelectedCrop(), s.farmEconomySystem_, s.farmDateSystem_.GetDay());
        if (result.Succeeded()) s.farmDocumentSystem_.MarkDirty();
        s.RouteFarmToolFeedback(result);
        status_ = result.Succeeded() ? Label::Success : Label::Failure;
        return;
    }
    case Action::FlowContinue:
        if (flow_.GetPhase() == FarmPlayFlow::Phase::Briefing) FrameFarm(s);
        flow_.Continue(); observation_ = false; pickingSlot_ = -1; focusedItem_ = -1;
        return;
    case Action::FlowRecords: open_ = true; page_ = 2; focusedItem_ = -1; return;
    case Action::SoilCare: open_ = true; page_ = kSoilCarePage; focusedItem_ = -1; status_ = Label::SoilCare; return;
    case Action::Quality:
        if (request.argument < 0 || request.argument > 1) return;
        open_ = true; page_ = request.argument == 1 ? kQualityHarvestPage : kQualityCurrentPage;
        focusedItem_ = -1; status_ = Label::Paused; return;
    case Action::Menu: open_ = true; if (observation_) page_ = 3; else if (terrain_) page_ = 1; focusedItem_ = -1; s.farmCropSelectionSystem_.Cancel(); return;
    case Action::ObserveField: EnterObservation(s); return;
    case Action::ObserveExit:
        observation_ = false; pickingSlot_ = -1; paused_ = pauseBeforeObservation_;
        if (!s.usePlayerCamera_) FrameFarm(s);
        return;
    case Action::PickSlot:
        if (request.argument == -1) { pickingSlot_ = -1; return; }
        if (request.argument < 0 || request.argument > 1 || s.farmGrowthComparisonSystem_.GetView(s.farmGrid_).status == FarmComparisonStatus::Running) return;
        EnterObservation(s); pickingSlot_ = request.argument; paused_ = true; return;
    case Action::JumpSlot: {
        if (request.argument < 0 || request.argument > 1) return;
        const auto comparison = s.farmGrowthComparisonSystem_.GetView(s.farmGrid_);
        const int index = comparison.rows[request.argument].tileIndex;
        if (comparison.status != FarmComparisonStatus::Invalidated && s.farmGrid_.GetTile(index)) {
            s.farmGrid_.SetSelectedIndex(index); pickingSlot_ = -1;
        }
        return;
    }
    case Action::Close: open_ = false; pending_ = Action::None; return;
    case Action::Tab:
        page_ = std::clamp(request.argument, 0, 4); focusedItem_ = -1; status_ = Label::Ready;
        if (terrain_ && page_ != 1) LeaveTerrain(s);
        if (page_ < 3 && observation_) {
            observation_ = false; pickingSlot_ = -1; paused_ = pauseBeforeObservation_;
            if (!s.usePlayerCamera_) FrameFarm(s);
        }
        return;
    case Action::Cancel:
        if (pending_ != Action::None) { pending_ = Action::None; return; }
        s.farmIrrigationPreviewSystem_.Cancel(); brush_ = 0; success = true; break;
    case Action::Load:
    {
        const auto& docs = s.farmDocumentSystem_.GetDocuments();
        std::vector<std::string> names;
        for (const auto& doc : docs) names.push_back(doc.displayName + "  /  " + doc.savedAt);
        const auto result = farmui::ShowRecordDialog(s.framework_->GetWinApp()->GetHwnd(),
            farmui::RecordDialogMode::ProgressPicker, names);
        if (result.action != farmui::RecordDialogAction::Load || result.selected < 0 || result.selected >= static_cast<int>(docs.size())) return;
        documentIndex_ = result.selected;
        pendingDocumentId_ = docs[documentIndex_].id;
        pending_ = Action::Load; return;
    }
    case Action::Restart:
    case Action::Exit:
        pending_ = request.action; open_ = true; return;
    case Action::ChangePlayMode:
        if (request.argument != 0 && request.argument != 1) return;
        pendingPlayMode_=request.argument; pending_=request.action; open_=true; return;
    case Action::LayoutLibrary: OpenLayoutLibrary(s); return;
    case Action::Accept: {
        const Action action = pending_; pending_ = Action::None;
        if (action == Action::None) return;
        if (action == Action::ChangePlayMode) {
            editor::GamePlayEditorCommand command{};
            command.type=C::SetFarmProgressionMode; command.farmGeneration=s.farmGrid_.GetGeneration();
            command.progressionMode=pendingPlayMode_==1 ? FarmProgressionMode::ContestSeason : FarmProgressionMode::Trial;
            success=s.gamePlayEditorBridge_.Execute(command);
            if (success) {
                flow_.Reset(); open_=false; observation_=false; terrain_=false; pickingSlot_=-1; paused_=false;
                focusedItem_=-1; s.farmCropSelectionSystem_.Cancel();
            }
            status_=success ? Label::Success : Label::Failure;
            return;
        }
        if (action == Action::SubmitContest) {
            editor::GamePlayEditorCommand command{};
            command.type=C::SubmitContestHarvest; command.farmGeneration=s.farmGrid_.GetGeneration();
            command.harvestRecordId=pendingSubmission_.argument; command.inventoryGeneration=pendingSubmission_.inventoryGeneration;
            command.contestDay=pendingContestDay_;
            success=s.gamePlayEditorBridge_.Execute(command);
            status_=success ? Label::ContestSubmitted : Label::Failure;
            if(success) page_=kContestResultsPage;
            pendingSubmission_={}; pendingContestDay_=0; focusedItem_=-1;
            return;
        }
        if (action == Action::Exit) { PostQuitMessage(0); return; }
        s.farmIrrigationPreviewSystem_.Cancel(); s.farmCropSelectionSystem_.Cancel(); brush_ = 0;
        if (action == Action::Restart) { s.ResetFarmSession(); success = true; }
        else {
            editor::FarmDocumentCommand command{};
            command.type = editor::FarmDocumentCommandType::Load;
            command.documentId = pendingDocumentId_;
            success = s.gamePlayEditorBridge_.Execute(command);
        }
        if (success) {
            s.farmGrowthComparisonSystem_.Reset(); s.InitializeTimeline();
            flow_.Reset(); open_ = false; observation_ = false; terrain_ = false; pickingSlot_ = -1; paused_ = false;
            status_ = action == Action::Restart ? Label::Success : Label::Loaded;
        }
        else status_ = Label::Failure;
        return;
    }
    case Action::HarvestDisplay:
        if (pending_ != Action::None || s.farmIrrigationPreviewSystem_.IsActive() || s.timelineScrubbing_ || !s.farmSeedShopRenderer_.IsReady()) return;
        open_ = true; page_ = kHarvestDisplayPage; harvestDisplay_.Reset(); focusedItem_ = -1; return;
    case Action::HarvestDisplaySelect:
        if (open_ && page_ == kHarvestDisplayPage && pending_ == Action::None)
            static_cast<void>(harvestDisplay_.Select(s.farmEconomySystem_, request.argument, request.inventoryGeneration));
        return;
    case Action::HarvestDisplayPage:
        if (open_ && page_ == kHarvestDisplayPage && pending_ == Action::None) {
            harvestDisplay_.MovePage(s.farmEconomySystem_, request.argument); focusedItem_ = -1;
        }
        return;
    case Action::HarvestInventory:
        open_ = true; page_ = kHarvestInventoryPage; harvestPage_ = 0; focusedItem_ = -1; return;
    case Action::ContestPreview:
        if (!open_ || (page_ != kHarvestDisplayPage && page_ != kHarvestInventoryPage && page_ != kContestPreviewPage && page_ != kContestEntryPage && page_ != kContestResultsPage)) return;
        if (request.argument != 0 && request.argument != 1) return;
        page_ = request.argument == 1 ? kContestEntryPage : kContestPreviewPage; focusedItem_ = -1; return;
    case Action::ContestResults:
        if(!open_ || page_!=kContestEntryPage) return;
        page_=kContestResultsPage; focusedItem_=-1; return;
    case Action::SubmitContest:
        if(!open_ || page_!=kContestEntryPage || pending_!=Action::None ||
            FarmContestSubmissionSystem::Evaluate(s.farmEconomySystem_,s.farmDateSystem_.GetDay())!=FarmContestSubmissionStatus::Ready) return;
        pending_=Action::SubmitContest; pendingSubmission_=request; pendingContestDay_=s.farmDateSystem_.GetDay(); focusedItem_=-1; return;
    case Action::HarvestPage:
        if (page_ == kHarvestInventoryPage && (request.argument == -1 || request.argument == 1))
            harvestPage_ = std::clamp(harvestPage_ + request.argument, 0, static_cast<int>(FarmEconomySystem::kMaxHarvestRecords));
        return;
    case Action::ProtectHarvest:
    case Action::UnprotectHarvest: {
        if (!open_ || (page_ != kHarvestInventoryPage && page_ != kHarvestDisplayPage)) return;
        editor::GamePlayEditorCommand command{};
        command.type = C::SetHarvestProtection; command.farmGeneration = s.farmGrid_.GetGeneration();
        command.harvestRecordId = request.argument; command.inventoryGeneration = request.inventoryGeneration;
        command.harvestProtected = request.action == Action::ProtectHarvest;
        status_ = s.gamePlayEditorBridge_.Execute(command) ? Label::ProtectionCommitted : Label::Failure;
        return;
    }
    case Action::ReserveContestHarvest:
    case Action::CancelContestReservation: {
        if (!open_ || (page_ != kHarvestInventoryPage && page_ != kHarvestDisplayPage)) return;
        editor::GamePlayEditorCommand command{};
        command.type = request.action == Action::ReserveContestHarvest ? C::ReserveContestHarvest : C::CancelContestReservation;
        command.farmGeneration = s.farmGrid_.GetGeneration();
        command.harvestRecordId = request.argument; command.inventoryGeneration = request.inventoryGeneration;
        status_ = s.gamePlayEditorBridge_.Execute(command) ? Label::ContestChanged : Label::Failure;
        return;
    }
    case Action::Previous: --documentIndex_; return;
    case Action::Next: ++documentIndex_; return;
    case Action::Save:
    case Action::SaveCopy: {
        if (s.farmIrrigationPreviewSystem_.IsActive()) break;
        editor::FarmDocumentCommand command{};
        command.type = request.action == Action::Save ? editor::FarmDocumentCommandType::Save : editor::FarmDocumentCommandType::SaveAs;
        if (request.action == Action::SaveCopy) {
            const auto result = farmui::ShowRecordDialog(s.framework_->GetWinApp()->GetHwnd(),
                farmui::RecordDialogMode::Name, {}, s.farmDocumentSystem_.GetDisplayName());
            if (result.action != farmui::RecordDialogAction::SaveNew) return;
            command.displayName = result.name;
        }
        success = s.gamePlayEditorBridge_.Execute(command);
        if (!success) farmui::RecordNotice(s.framework_->GetWinApp()->GetHwnd(), s.farmDocumentSystem_.GetStatusMessage());
        status_ = success ? Label::Saved : Label::Failure; return;
    }
    case Action::Pause: paused_ = !paused_; if (!paused_) pickingSlot_ = -1; return;
    case Action::Follow: s.SetUsePlayerCamera(true); open_ = false; return;
    case Action::Overview: {
        if (FrameFarm(s)) open_ = false;
        else status_ = Label::Failure;
        return;
    }
    default: break;
    }
    if (request.action == Action::Confirm) { success = dispatch(C::ConfirmFarmIrrigationPreview); if (success) brush_ = 0; }
    else if (request.action == Action::Stop) { success = s.farmGrowthComparisonSystem_.Stop(); if (success && observation_) paused_ = true; }
    else if (request.action == Action::Clear) { s.farmGrowthComparisonSystem_.Reset(); pickingSlot_ = -1; success = true; }
    else if (s.farmProgressionSystem_.IsCleared() || s.farmIrrigationPreviewSystem_.IsActive()) {
        if (request.action != Action::Cancel) { status_ = Label::Failure; return; }
    } else {
        switch (request.action) {
        case Action::SetIrrigation: {
            if (request.argument != 0 && request.argument != 1) break;
            editor::GamePlayEditorCommand command{};
            command.type = C::SetFarmIrrigation;
            command.farmGeneration = s.farmGrid_.GetGeneration();
            command.farmTileIndex = s.farmGrid_.GetSelectedIndex();
            command.irrigationEnabled = request.argument == 1;
            success = s.gamePlayEditorBridge_.Execute(command);
            break;
        }
        case Action::Compost:
            success = dispatch(C::CompostFarmTile);
            status_ = success ? Label::SoilApplied : Label::Failure;
            return;
        case Action::Tool: {
            constexpr FarmTool tools[] = {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest};
            if (request.argument < 0 || request.argument >= 4) break;
            s.farmToolSystem_.SetTool(tools[request.argument]); open_ = false; success = true; break;
        }
        case Action::Crop: {
            if (request.argument < 0 || request.argument > 1) break;
            const auto previous = s.farmCropSelectionSystem_.GetSelectedCrop();
            success = s.farmCropSelectionSystem_.SetSelectedCrop(farm::CropTypeFromSlot(request.argument));
            if (success && previous != s.farmCropSelectionSystem_.GetSelectedCrop()) s.farmDocumentSystem_.MarkDirty();
            s.farmFeedbackSystem_.ShowCropSelected(s.farmCropSelectionSystem_.GetSelectedCrop()); break;
        }
        case Action::Buy:
        case Action::BuyAndReturn: {
            const auto result = s.farmEconomySystem_.BuySeed(s.farmCropSelectionSystem_.GetSelectedCrop());
            success = result.Succeeded();
            if (success) { s.farmDocumentSystem_.MarkDirty(); s.farmFeedbackSystem_.ShowSeedPurchased(result.crop, result.purchasedCount, result.spentMoney); }
            else s.farmFeedbackSystem_.ShowInsufficientMoney();
            if (success && request.action == Action::BuyAndReturn) {
                s.farmToolSystem_.SetTool(FarmTool::Seed); open_ = false;
            }
            break;
        }
        case Action::Sell: case Action::SellAll:
            s.RouteFarmSale(request.action == Action::SellAll ? s.farmEconomySystem_.SellAll() : s.farmEconomySystem_.SellCrop(s.farmCropSelectionSystem_.GetSelectedCrop()));
            open_ = false; success = true; break;
        case Action::Raise: success = dispatch(C::BeginFarmRaiseTerrainPreview); break;
        case Action::Lower: success = dispatch(C::BeginFarmLowerTerrainPreview); break;
        case Action::Canal: success = dispatch(C::BeginFarmCanalPreview); break;
        case Action::Source: success = dispatch(C::BeginFarmWaterSourcePreview); break;
        case Action::Path: success = dispatch(C::BeginFarmCanalPathPreview); if (success) brush_ = 1; break;
        case Action::RemovePath: success = dispatch(C::BeginFarmCanalRemovalPathPreview); if (success) brush_ = 2; break;
        case Action::Undo: success = dispatch(C::UndoFarmEdit); break;
        case Action::Redo: success = dispatch(C::RedoFarmEdit); break;
        case Action::PinA: case Action::PinB:
            success = s.farmGrowthComparisonSystem_.Pin(s.farmGrid_, request.action == Action::PinA ? 0 : 1, s.farmGrid_.GetSelectedIndex());
            if (success) {
                EnterObservation(s);
                const int other = request.action == Action::PinA ? 1 : 0;
                const auto comparison = s.farmGrowthComparisonSystem_.GetView(s.farmGrid_);
                pickingSlot_ = comparison.rows[other].tileIndex < 0 ? other : -1;
                paused_ = true;
            }
            break;
        case Action::Start:
            success = s.farmGrowthComparisonSystem_.Start(s.farmGrid_);
            if (success) { EnterObservation(s); pickingSlot_ = -1; paused_ = false; }
            break;
        case Action::Speed:
            if (request.argument != 1 && request.argument != 2 && request.argument != 4) break;
            s.farmDateSystem_.SetTimeScale(static_cast<float>(request.argument));
            s.farmDocumentSystem_.MarkDirty();
            pickingSlot_ = -1; paused_ = false; success = true; break;
        default: break;
        }
        if (s.farmIrrigationPreviewSystem_.IsActive()) EnterTerrain(s);
    }
    status_ = success ? Label::Success : Label::Failure;
}

bool FarmRuntimeController::FrameFarm(GamePlayScene& s, bool followPlayer) {
    if (!s.camera_ || s.farmGrid_.GetWidth() <= 0 || s.farmGrid_.GetHeight() <= 0) return false;
    const auto& layout = s.farmVisualSystem_.GetLayout();
    const float pitch = layout.tileSize + layout.tileGap;
    const float halfX = static_cast<float>(s.farmGrid_.GetWidth())*pitch*0.5f;
    const float halfZ = static_cast<float>(s.farmGrid_.GetHeight())*pitch*0.5f;
    // Include every editable height and modest crop headroom, not only the current two corner tiles.
    constexpr float cropHeadroom = 0.8f;
    const Vector3 minimum{layout.center.x-halfX, layout.center.y +
        FarmToolActionSystem::kMinimumHeightLevel*layout.heightStep, layout.center.z-halfZ};
    const Vector3 maximum{layout.center.x+halfX, layout.center.y +
        FarmToolActionSystem::kMaximumHeightLevel*layout.heightStep+cropHeadroom, layout.center.z+halfZ};
    const auto& projection = s.camera_->GetProjectionMatrix();
    farm::OverviewPose pose;
    const auto frame = terrain_ ? farm::kTerrainOverviewFrame :
        observation_ ? farm::kObservationOverviewFrame : farm::kFarmOverviewFrame;
    if (followPlayer) {
        if (!s.levelGameplay_.HasPlayer() || !farm::FitFarmFollowCamera(minimum, maximum,
            s.levelGameplay_.GetPlayerColliderCenter(), s.levelGameplay_.GetPlayerColliderHalfExtents(),
            projection.m[0][0], projection.m[1][1], s.camera_->GetNearClip(), s.camera_->GetFarClip(), frame, pose)) return false;
    } else {
        if (!farm::FitOverviewCamera(minimum, maximum, projection.m[0][0], projection.m[1][1],
            s.camera_->GetNearClip(), s.camera_->GetFarClip(), frame, pose)) return false;
        s.SetUsePlayerCamera(false);
    }
    s.cameraPos_ = pose.position; s.cameraRot_ = pose.rotation;
    if (!followPlayer) { s.debugCameraPos_ = pose.position; s.debugCameraRot_ = pose.rotation; }
    return true;
}

int FarmRuntimeController::ResolveHoveredTile(const GamePlayScene& s) const {
    if (!ready_ || !s.viewportFocused_ || !s.viewportHovered_ || open_ || observation_ ||
        frameCaptured_ || flow_.BlocksSimulation() || s.farmProgressionSystem_.IsCleared() ||
        s.farmIrrigationPreviewSystem_.IsActive() || s.farmCropSelectionSystem_.IsOpen() ||
        view_.Covers(pointer_) || (!terrain_ && farmui::View::FarmHUDCovers(pointer_))) return -1;
    Vector3 origin{}, direction{};
    int index = -1;
    if (!s.TryBuildViewportRay(origin, direction) ||
        !s.farmVisualSystem_.TryPickTile(s.farmGrid_, origin, direction, index)) return -1;
    return index;
}

void FarmRuntimeController::EnterObservation(GamePlayScene& s) {
    if (terrain_) LeaveTerrain(s);
    const bool entering = !observation_;
    if (entering) { pauseBeforeObservation_ = paused_; paused_ = true; }
    observation_ = true; open_ = false; focusedItem_ = -1;
    if (entering) FrameFarm(s);
    s.farmCropSelectionSystem_.Cancel();
}

void FarmRuntimeController::EnterTerrain(GamePlayScene& s) {
    const bool entering = !terrain_;
    if (observation_) {
        observation_ = false; pickingSlot_ = -1; paused_ = pauseBeforeObservation_;
    }
    terrain_ = true; open_ = false; focusedItem_ = -1;
    s.farmCropSelectionSystem_.Cancel();
    if (entering) { status_ = Label::Terrain; FrameFarm(s); }
}

void FarmRuntimeController::LeaveTerrain(GamePlayScene& s) {
    // Uncommitted edits are discarded; the player's explicit pause setting is unchanged.
    s.farmIrrigationPreviewSystem_.Cancel(); brush_ = 0;
    terrain_ = false;
    if (!s.usePlayerCamera_) FrameFarm(s);
}

bool FarmRuntimeController::UpdateTerrain(GamePlayScene& s, const Input& input, bool click) {
    const int dx = (input.TriggerKey(InputKey::ArrowRight) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowLeft) ? 1 : 0);
    const int dy = (input.TriggerKey(InputKey::ArrowDown) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowUp) ? 1 : 0);
    if (dx || dy) s.farmGrid_.MoveSelection(dx, dy);
    if (click && !view_.Covers(pointer_)) {
        Vector3 origin{}, direction{}; int index = -1;
        if (s.TryBuildViewportRay(origin, direction) && s.farmVisualSystem_.TryPickTile(s.farmGrid_, origin, direction, index))
            s.farmGrid_.SetSelectedIndex(index);
    }
    Action action = Action::None;
    if (input.TriggerKey(InputKey::N)) action = Action::Canal;
    if (input.TriggerKey(InputKey::M)) action = Action::Source;
    if (input.TriggerKey(InputKey::PageUp)) action = Action::Raise;
    if (input.TriggerKey(InputKey::PageDown)) action = Action::Lower;
    if (action != Action::None) Execute(s, {action});
    BuildView(s);
    return true;
}

bool FarmRuntimeController::UpdateObservation(GamePlayScene& s, const Input& input, bool click) {
    // Observation captures gameplay input, but only explicit pause blocks simulation.
    // Panel backgrounds and disabled buttons must not pick a tile behind the UI.
    if (input.TriggerMouseButton(InputMouseButton::Right)) { pickingSlot_ = -1; BuildView(s); return true; }
    const int dx = (input.TriggerKey(InputKey::ArrowRight) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowLeft) ? 1 : 0);
    const int dy = (input.TriggerKey(InputKey::ArrowDown) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowUp) ? 1 : 0);
    if (dx || dy) { s.farmGrid_.MoveSelection(dx, dy); BuildView(s); }
    if (pickingSlot_ >= 0 && input.TriggerKey(InputKey::Enter)) {
        Execute(s, {pickingSlot_ == 0 ? Action::PinA : Action::PinB}); BuildView(s); frameCaptured_ = true; return true;
    }
    if (view_.Covers(pointer_)) return true;
    if (click) {
        Vector3 origin{}, direction{}; int index = -1;
        if (s.TryBuildViewportRay(origin, direction) && s.farmVisualSystem_.TryPickTile(s.farmGrid_, origin, direction, index)) {
            s.farmGrid_.SetSelectedIndex(index);
            if (pickingSlot_ >= 0) Execute(s, {pickingSlot_ == 0 ? Action::PinA : Action::PinB});
            BuildView(s); frameCaptured_ = true;
        }
    }
    return true;
}

void FarmRuntimeController::OpenLayoutLibrary(GamePlayScene& s) {
    if (!s.framework_ || !s.framework_->GetWinApp() || s.farmIrrigationPreviewSystem_.IsActive()) return;
    const HWND owner = s.framework_->GetWinApp()->GetHwnd();
    FarmLayoutSystem layouts;
    if (!layouts.Initialize("Settings/farm_layouts")) { farmui::RecordNotice(owner, layouts.Error()); return; }
    s.farmCropSelectionSystem_.Cancel();
    std::string name;
    for (;;) {
        std::vector<std::string> names;
        for (const auto& entry : layouts.Entries()) names.push_back(entry.name);
        const auto result = farmui::ShowRecordDialog(owner, farmui::RecordDialogMode::LayoutLibrary, names, name);
        if (result.action == farmui::RecordDialogAction::Cancel) return;
        if (result.action == farmui::RecordDialogAction::SaveNew) {
            name = result.name;
            if (!layouts.SaveNew(name, s.farmGrid_)) farmui::RecordNotice(owner, layouts.Error());
            else { farmui::RecordNotice(owner, "配置を保存しました。進行セーブは変更していません。"); name.clear(); }
            continue;
        }
        if (result.selected < 0 || result.selected >= static_cast<int>(layouts.Entries().size())) continue;
        if (!farmui::ConfirmLayoutReplacement(owner)) continue;
        if (!layouts.Load(layouts.Entries()[result.selected].id, s.farmGrid_)) {
            farmui::RecordNotice(owner, layouts.Error()); continue;
        }
        s.farmToolActionSystem_.ClearHistory();
        s.farmIrrigationPreviewSystem_.Cancel();
        s.farmGrowthComparisonSystem_.Reset();
        s.farmFeedbackSystem_.Clear();
        s.farmIrrigationSystem_.Rebuild(s.farmGrid_);
        s.farmDocumentSystem_.MarkDirty();
        s.InitializeTimeline();
        brush_ = 0;
        status_ = Label::Loaded;
        farmui::RecordNotice(owner, "配置を再現しました。所持金・持ち物・日数はそのままです。");
        return;
    }
}

bool FarmRuntimeController::Update(GamePlayScene& s, const Input& input) {
    frameCaptured_ = false;
    if (!ready_) return false;
    if (!s.viewportFocused_) {
        s.farmIrrigationPreviewSystem_.EndTerrainStroke();
        frameCaptured_ = true; return true;
    }
    s.ConvertMouseToVirtualScreen(input.GetMousePosition(), pointer_);
    BuildView(s);
    const bool click = input.TriggerMouseButton(InputMouseButton::Left);
    const auto hit = view_.Hit(pointer_);
    // Shop input never falls through to farm picking, quick purchases or player movement.
    if (view_.seedShop) {
        if (input.TriggerKey(InputKey::Escape) || input.TriggerMouseButton(InputMouseButton::Right)) {
            if (shop_.GetPhase() == FarmSeedShopSystem::Phase::Confirming) shop_.CancelConfirmation();
            else shop_.Close();
        } else if (click && hit.action != Action::None) Execute(s, hit);
        else {
            if (input.TriggerKey(InputKey::ArrowLeft)) shop_.Select((shop_.Selection() + FarmSeedShopSystem::kProductCount - 1) % FarmSeedShopSystem::kProductCount);
            else if (input.TriggerKey(InputKey::ArrowRight)) shop_.Select((shop_.Selection() + 1) % FarmSeedShopSystem::kProductCount);
            if (input.TriggerKey(InputKey::ArrowUp)) shop_.ChangeQuantity(1);
            else if (input.TriggerKey(InputKey::ArrowDown)) shop_.ChangeQuantity(-1);
            if (input.TriggerKey(InputKey::Enter)) Execute(s, {shop_.GetPhase() == FarmSeedShopSystem::Phase::Confirming ? Action::ShopConfirm : Action::ShopReview});
        }
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (click && hit.action != Action::None) {
        s.farmIrrigationPreviewSystem_.EndTerrainStroke();
        focusedItem_ = -1;
        Execute(s, hit); BuildView(s); frameCaptured_ = true; return true;
    }
    if (!contestNotice_ && input.TriggerKey(InputKey::Escape)) {
        if (pending_ != Action::None) pending_ = Action::None;
        else if (open_) open_ = false;
        else if (observation_ && pickingSlot_ >= 0) pickingSlot_ = -1;
        else if (s.farmIrrigationPreviewSystem_.IsActive()) { s.farmIrrigationPreviewSystem_.Cancel(); brush_ = 0; }
        else if (terrain_) LeaveTerrain(s);
        else if (s.farmCropSelectionSystem_.IsOpen()) s.farmCropSelectionSystem_.Cancel();
        else open_ = true;
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (contestNotice_ || open_ || flow_.BlocksSimulation()) {
        s.farmIrrigationPreviewSystem_.EndTerrainStroke();
        if (!contestNotice_ && open_ && input.TriggerKey(InputKey::Tab) && pending_ == Action::None) {
            Execute(s, {Action::Tab, (page_ + 1) % 5});
        }
        const int step = (input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::ArrowRight)) ? 1
            : (input.TriggerKey(InputKey::ArrowUp) || input.TriggerKey(InputKey::ArrowLeft)) ? -1 : 0;
        if (step != 0) {
            focusedItem_ = view_.NextActionable(focusedItem_, step);
        }
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) {
            const auto& item = view_.items[focusedItem_];
            if (step) pointer_ = {item.rect.x + item.rect.width * 0.5f, item.rect.y + item.rect.height * 0.5f};
            if (input.TriggerKey(InputKey::Enter) && item.enabled) {
                Execute(s, item.request); focusedItem_ = -1;
            }
        }
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (observation_) return UpdateObservation(s, input, click);
    if (click && view_.Covers(pointer_)) {
        s.farmIrrigationPreviewSystem_.EndTerrainStroke();
        frameCaptured_ = true; return true;
    }
    if (s.farmIrrigationPreviewSystem_.IsActive()) {
        auto& preview = s.farmIrrigationPreviewSystem_;
        const auto operation = preview.GetOperation();
        const bool heightBrush = operation == farm::FarmIrrigationPreviewOperation::RaiseTerrain ||
            operation == farm::FarmIrrigationPreviewOperation::LowerTerrain;
        if (input.TriggerKey(InputKey::Enter)) Execute(s, {Action::Confirm});
        else if (input.TriggerMouseButton(InputMouseButton::Right)) Execute(s, {Action::Cancel});
        else if ((brush_ || heightBrush) && !view_.Covers(pointer_) && input.PushMouseButton(InputMouseButton::Left)) {
            Vector3 origin{}, direction{}; int index = -1;
            if (s.TryBuildViewportRay(origin, direction) && s.farmVisualSystem_.TryPickTile(s.farmGrid_, origin, direction, index)) {
                if (heightBrush) static_cast<void>(preview.VisitTerrainTile(s.farmGrid_, index));
                else static_cast<void>(preview.VisitCanalPathTile(s.farmGrid_, index));
            } else preview.EndTerrainStroke();
        } else preview.EndTerrainStroke();
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (terrain_) return UpdateTerrain(s, input, click);
    if (!s.farmCropSelectionSystem_.IsOpen()) {
        if (click) {
            for (int i = 0; i < 4; ++i) {
                if (farmui::Rect{408.0f + 148 * i, 622, 140, 58}.Contains(pointer_)) {
                    Execute(s, {Action::Tool, i}); BuildView(s); frameCaptured_ = true; return true;
                }
            }
            // A HUD click must never select or modify a tile hidden behind it.
            if (farmui::View::FarmHUDCovers(pointer_)) { frameCaptured_ = true; return true; }
        }
        Action action = Action::None;
        if (input.TriggerKey(InputKey::N)) action = Action::Canal;
        if (input.TriggerKey(InputKey::M)) action = Action::Source;
        if (input.TriggerKey(InputKey::PageUp)) action = Action::Raise;
        if (input.TriggerKey(InputKey::PageDown)) action = Action::Lower;
        if (action != Action::None) { Execute(s, {action}); BuildView(s); frameCaptured_ = true; return true; }
    }
    if (paused_) {
        // Pause freezes simulation, not explicit farm edits. Consume this frame so
        // Scene cannot repeat the action or move the player after the edit.
        if (s.farmProgressionSystem_.IsCleared() || s.farmCropSelectionSystem_.IsOpen()) return true;
        if (click && s.viewportHovered_) {
            Vector3 origin{}, direction{}; int index = -1;
            if (s.TryBuildViewportRay(origin, direction) &&
                s.farmVisualSystem_.TryPickTile(s.farmGrid_, origin, direction, index))
                s.farmGrid_.SetSelectedIndex(index);
        }
        FarmInputContext context{};
        context.currentDay = s.farmDateSystem_.GetDay();
        const auto result = s.farmInputSystem_.Update(input, context, s.farmGrid_, s.farmToolSystem_,
            s.farmCropSelectionSystem_.GetSelectedCrop(), s.farmEconomySystem_, s.farmToolActionSystem_);
        if (result.contentChanged) s.farmDocumentSystem_.MarkDirty();
        s.RouteFarmToolFeedback(result.toolAction);
        if (result.buySeedRequested) Execute(s, {Action::Buy});
        if (result.sellSelectedRequested) Execute(s, {Action::Sell});
        else if (result.sellRequested) Execute(s, {Action::SellAll});
        BuildView(s);
        return true;
    }
    return false;
}
