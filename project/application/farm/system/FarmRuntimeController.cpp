#include "farm/system/FarmRuntimeController.h"
#include "scene/GamePlayScene.h"
#include "io/Input.h"
#include "farm/system/FarmLayoutSystem.h"
#include "farm/ui/FarmRecordDialog.h"
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
template<class... Args> std::string Numbers(const char* format, Args... args) {
    char buffer[96]{};
    std::snprintf(buffer, sizeof(buffer), format, args...);
    return buffer;
}
}

void FarmRuntimeController::BuildView(GamePlayScene& s) {
    view_ = {};
    view_.modal = open_;
    if (!open_) {
        view_.Add(Label::Menu, {1040, 654, 216, 42}, {Action::Menu});
        if (s.farmIrrigationPreviewSystem_.IsActive()) {
            view_.Add(Label::Confirm, {1040, 544, 216, 44}, {Action::Confirm}, s.farmIrrigationPreviewSystem_.CanConfirm(s.farmGrid_));
            view_.Add(Label::Cancel, {1040, 596, 216, 44}, {Action::Cancel});
        } else if (paused_) view_.Add(Label::Paused, {420, 32, 350, 42});
        return;
    }
    constexpr Label tabs[] = {Label::Farm, Label::Terrain, Label::Records, Label::Observe, Label::Settings};
    for (int i = 0; i < 5; ++i) view_.Add(tabs[i], {150.0f + i * 196, 52, 188, 48}, {Action::Tab, i}, pending_ == Action::None, page_ == i);
    view_.Add(Label::Resume, {916, 628, 214, 44}, {Action::Close});
    view_.Add(status_, {166, 628, 726, 42});
    if (pending_ != Action::None) {
        view_.Add(Label::ConfirmDestructive, {256, 250, 760, 54});
        view_.Add(Label::Accept, {256, 342, 360, 56}, {Action::Accept});
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
        button(Label::Turnip, Action::Crop, 0, unlocked, crop == farm::CropType::TestCrop);
        button(Label::Carrot, Action::Crop, 1, unlocked, crop == farm::CropType::Carrot);
        button(Label::Buy, Action::Buy, 0, unlocked && hud.money >= hud.seedPrice);
        button(Label::Sell, Action::Sell, 0, unlocked && s.farmEconomySystem_.GetCropCount(crop) > 0);
        button(Label::SellAll, Action::SellAll, 0, unlocked && hud.cropCount > 0);
        view_.Value(Label::Money, 454, Numbers("%dG / %dG", hud.money, hud.seedPrice));
        view_.Value(Label::Inventory, 500, Numbers("%d / %d / %dG", hud.seedCount, s.farmEconomySystem_.GetCropCount(crop), s.farmEconomySystem_.GetCropInventoryValue(crop)));
        view_.Value(Label::Tile, 546, Numbers("%d / H%d / %d / %d", hud.selectedTileIndex, hud.selectedTileHeight, hud.selectedTileMoisturePercent, hud.selectedTileGrowthPercent));
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
        view_.Value(Label::Tile, 454, Numbers("%d / H%d / %d / %d", hud.selectedTileIndex, hud.selectedTileHeight, hud.selectedTileMoisturePercent, hud.selectedTileGrowthPercent));
        view_.Value(Label::WaterStats, 500, Numbers("%d / %d", s.farmIrrigationSystem_.GetSuppliedCanalCount(), s.farmIrrigationSystem_.GetIrrigationRangeTileCount()));
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
        button(Label::PinA, Action::PinA, 0, unlocked && tile);
        button(Label::PinB, Action::PinB, 0, unlocked && tile);
        button(Label::Start, Action::Start, 0, unlocked && comparison.startIssue == FarmComparisonIssue::None);
        button(Label::Stop, Action::Stop, 0, comparison.status == FarmComparisonStatus::Running);
        button(Label::Clear, Action::Clear);
        view_.Value(Label::Compare, 330, Numbers("%d / %d / %.1f", comparison.rows[0].tileIndex, comparison.rows[1].tileIndex, comparison.elapsedSeconds));
        view_.Add(comparison.status == FarmComparisonStatus::Running ? Label::Running : Label::NotRunning, {176, 374, 500, 38});
        if (tile && farm::IsPlantableCrop(tile->crop)) {
            const auto quality = s.farmToolActionSystem_.EvaluateHarvestQuality(*tile);
            view_.Value(Label::Quality, 418, Numbers("%.0f / %.0f / %.0f", quality.maturity * 100, quality.waterBalance * 100, quality.terrainFit * 100));
            view_.Value(Label::Score, 462, Numbers("%d / %dG", quality.score, quality.salePrice));
        } else view_.Add(Label::NoCrop, {176, 418, 850, 38});
        view_.Value(Label::Status, 506, Numbers("A %.0f/%.0f B %.0f/%.0f", comparison.rows[0].current.moisture * 100, comparison.rows[0].current.growth * 100, comparison.rows[1].current.moisture * 100, comparison.rows[1].current.growth * 100));
        if (comparison.startIssue != FarmComparisonIssue::None) view_.Add(Label::CompareIssue, {176, 550, 910, 38});
    } else {
        button(paused_ ? Label::Play : Label::Pause, Action::Pause);
        button(Label::Speed1, Action::Speed, 1, unlocked, !paused_ && hud.timeScale == 1);
        button(Label::Speed2, Action::Speed, 2, unlocked, !paused_ && hud.timeScale == 2);
        button(Label::Speed4, Action::Speed, 4, unlocked, !paused_ && hud.timeScale == 4);
        button(Label::Follow, Action::Follow, 0, true, s.usePlayerCamera_);
        button(Label::Overview, Action::Overview, 0, true, !s.usePlayerCamera_);
        button(Label::Exit, Action::Exit);
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
    case Action::Menu: open_ = true; focusedItem_ = -1; s.farmCropSelectionSystem_.Cancel(); return;
    case Action::Close: open_ = false; pending_ = Action::None; return;
    case Action::Tab: page_ = std::clamp(request.argument, 0, 4); focusedItem_ = -1; status_ = Label::Ready; return;
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
        pending_ = request.action; return;
    case Action::LayoutLibrary: OpenLayoutLibrary(s); return;
    case Action::Accept: {
        const Action action = pending_; pending_ = Action::None;
        if (action == Action::None) return;
        if (action == Action::Exit) { PostQuitMessage(0); return; }
        s.farmIrrigationPreviewSystem_.Cancel(); s.farmCropSelectionSystem_.Cancel(); brush_ = 0;
        if (action == Action::Restart) { s.ResetFarmSession(); success = true; }
        else {
            editor::FarmDocumentCommand command{};
            command.type = editor::FarmDocumentCommandType::Load;
            command.documentId = pendingDocumentId_;
            success = s.gamePlayEditorBridge_.Execute(command);
        }
        if (success) { s.farmGrowthComparisonSystem_.Reset(); s.InitializeTimeline(); status_ = action == Action::Restart ? Label::Success : Label::Loaded; }
        else status_ = Label::Failure;
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
    case Action::Pause: paused_ = !paused_; return;
    case Action::Follow: s.SetUsePlayerCamera(true); open_ = false; return;
    case Action::Overview: {
        const int count = s.farmGrid_.GetTileCount();
        if (count <= 0) break;
        const auto first = s.farmVisualSystem_.GetTileCenter(s.farmGrid_, 0);
        const auto last = s.farmVisualSystem_.GetTileCenter(s.farmGrid_, count - 1);
        // Keep the 5x4 field inside the space between the top and bottom HUD bands.
        constexpr float elevation = 16.0f;
        constexpr float setback = 20.0f;
        s.SetUsePlayerCamera(false);
        s.cameraPos_ = {(first.x + last.x) * 0.5f, (first.y + last.y) * 0.5f + elevation, (first.z + last.z) * 0.5f - setback};
        s.cameraRot_ = {std::atan2(elevation, setback), 0, 0};
        s.debugCameraPos_ = s.cameraPos_; s.debugCameraRot_ = s.cameraRot_; open_ = false; return;
    }
    default: break;
    }
    if (request.action == Action::Confirm) { success = dispatch(C::ConfirmFarmIrrigationPreview); if (success) brush_ = 0; }
    else if (request.action == Action::Stop) success = s.farmGrowthComparisonSystem_.Stop();
    else if (request.action == Action::Clear) { s.farmGrowthComparisonSystem_.Reset(); success = true; }
    else if (s.farmProgressionSystem_.IsCleared() || s.farmIrrigationPreviewSystem_.IsActive()) {
        if (request.action != Action::Cancel) { status_ = Label::Failure; return; }
    } else {
        switch (request.action) {
        case Action::Tool: {
            constexpr FarmTool tools[] = {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest};
            if (request.argument < 0 || request.argument >= 4) break;
            s.farmToolSystem_.SetTool(tools[request.argument]); open_ = false; success = true; break;
        }
        case Action::Crop: {
            if (request.argument < 0 || request.argument > 1) break;
            const auto previous = s.farmCropSelectionSystem_.GetSelectedCrop();
            success = s.farmCropSelectionSystem_.SetSelectedCrop(request.argument == 1 ? farm::CropType::Carrot : farm::CropType::TestCrop);
            if (success && previous != s.farmCropSelectionSystem_.GetSelectedCrop()) s.farmDocumentSystem_.MarkDirty();
            s.farmFeedbackSystem_.ShowCropSelected(s.farmCropSelectionSystem_.GetSelectedCrop()); break;
        }
        case Action::Buy: {
            const auto result = s.farmEconomySystem_.BuySeed(s.farmCropSelectionSystem_.GetSelectedCrop());
            success = result.Succeeded();
            if (success) { s.farmDocumentSystem_.MarkDirty(); s.farmFeedbackSystem_.ShowSeedPurchased(result.crop, result.purchasedCount, result.spentMoney); }
            else s.farmFeedbackSystem_.ShowInsufficientMoney();
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
            success = s.farmGrowthComparisonSystem_.Pin(s.farmGrid_, request.action == Action::PinA ? 0 : 1, s.farmGrid_.GetSelectedIndex()); break;
        case Action::Start: success = s.farmGrowthComparisonSystem_.Start(s.farmGrid_); if (success) { open_ = false; paused_ = false; } break;
        case Action::Speed: s.farmDateSystem_.SetTimeScale(static_cast<float>(std::clamp(request.argument, 1, 4))); paused_ = false; success = true; break;
        default: break;
        }
        if (s.farmIrrigationPreviewSystem_.IsActive()) { open_ = false; s.farmCropSelectionSystem_.Cancel(); }
    }
    status_ = success ? Label::Success : Label::Failure;
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
    if (!s.viewportFocused_) { frameCaptured_ = true; return true; }
    s.ConvertMouseToVirtualScreen(input.GetMousePosition(), pointer_);
    BuildView(s);
    const bool click = input.TriggerMouseButton(InputMouseButton::Left);
    const auto hit = view_.Hit(pointer_);
    if (click && hit.action != Action::None) {
        focusedItem_ = -1;
        Execute(s, hit); BuildView(s); frameCaptured_ = true; return true;
    }
    if (input.TriggerKey(InputKey::Escape)) {
        if (pending_ != Action::None) pending_ = Action::None;
        else if (open_) open_ = false;
        else if (s.farmIrrigationPreviewSystem_.IsActive()) { s.farmIrrigationPreviewSystem_.Cancel(); brush_ = 0; }
        else if (s.farmCropSelectionSystem_.IsOpen()) s.farmCropSelectionSystem_.Cancel();
        else open_ = true;
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (open_) {
        if (input.TriggerKey(InputKey::Tab) && pending_ == Action::None) {
            page_ = (page_ + 1) % 5; focusedItem_ = -1;
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
    if (paused_) return true;
    if (s.farmIrrigationPreviewSystem_.IsActive()) {
        if (input.TriggerKey(InputKey::Enter)) Execute(s, {Action::Confirm});
        else if (input.TriggerMouseButton(InputMouseButton::Right)) Execute(s, {Action::Cancel});
        else if (brush_ && input.PushMouseButton(InputMouseButton::Left)) {
            Vector3 origin{}, direction{}; int index = -1;
            if (s.TryBuildViewportRay(origin, direction) && s.farmVisualSystem_.TryPickTile(s.farmGrid_, origin, direction, index))
                static_cast<void>(s.farmIrrigationPreviewSystem_.VisitCanalPathTile(s.farmGrid_, index));
        }
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (!s.farmCropSelectionSystem_.IsOpen()) {
        if (click) {
            for (int i = 0; i < 4; ++i) {
                if (farmui::Rect{408.0f + 148 * i, 622, 140, 58}.Contains(pointer_)) {
                    Execute(s, {Action::Tool, i}); BuildView(s); frameCaptured_ = true; return true;
                }
            }
            // A HUD click must never select or modify a tile hidden behind it.
            constexpr farmui::Rect panels[] = {{24,24,310,132}, {876,24,380,228}, {24,520,350,176}, {390,548,628,148}};
            for (const auto& panel : panels) if (panel.Contains(pointer_)) { frameCaptured_ = true; return true; }
        }
        Action action = Action::None;
        if (input.TriggerKey(InputKey::N)) action = Action::Canal;
        if (input.TriggerKey(InputKey::M)) action = Action::Source;
        if (input.TriggerKey(InputKey::PageUp)) action = Action::Raise;
        if (input.TriggerKey(InputKey::PageDown)) action = Action::Lower;
        if (action != Action::None) { Execute(s, {action}); BuildView(s); frameCaptured_ = true; return true; }
    }
    return false;
}
