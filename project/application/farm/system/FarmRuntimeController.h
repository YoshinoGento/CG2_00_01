#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/system/FarmPlayFlow.h"

class GamePlayScene;
class Input;

// Routes Release controls to existing gameplay Systems; owns only menu/input state.
class FarmRuntimeController final {
public:
    bool Initialize(SpriteCommon* common) { ready_ = ui_.Initialize(common); return ready_; }
    bool Update(GamePlayScene& scene, const Input& input);
    void Draw() { ui_.Draw(view_, pointer_); }
    bool BlocksSimulation() const noexcept { return open_ || paused_ || terrain_ || frameCaptured_ || (ready_ && flow_.BlocksSimulation()); }
    bool ConsumesInput() const noexcept { return open_ || paused_ || terrain_ || observation_ || frameCaptured_ || (ready_ && flow_.BlocksSimulation()); }
    bool IsOpen() const noexcept { return open_; }
    bool PresentsFarmResult() const noexcept { return ready_ &&
        (flow_.GetPhase() == FarmPlayFlow::Phase::Result || flow_.GetPhase() == FarmPlayFlow::Phase::Review); }
    bool HidesFarmHUD() const noexcept { return open_ || observation_ || terrain_ || (ready_ && flow_.BlocksSimulation()); }
    [[nodiscard]] int ResolveHoveredTile(const GamePlayScene& scene) const;
    void Refresh(GamePlayScene& scene) { BuildView(scene); }
    void OpenLayoutLibrary(GamePlayScene& scene);
private:
    void BuildView(GamePlayScene& scene);
    void Execute(GamePlayScene& scene, farmui::Request request);
    void EnterObservation(GamePlayScene& scene);
    void EnterTerrain(GamePlayScene& scene);
    void LeaveTerrain(GamePlayScene& scene);
    bool UpdateTerrain(GamePlayScene& scene, const Input& input, bool click);
    bool FrameFarm(GamePlayScene& scene);
    bool UpdateObservation(GamePlayScene& scene, const Input& input, bool click);
    farmui::RuntimeUI ui_;
    FarmPlayFlow flow_;
    farmui::View view_;
    Vector2 pointer_{-1, -1};
    farmui::Action pending_ = farmui::Action::None;
    farmui::Label status_ = farmui::Label::Ready;
    std::string pendingDocumentId_;
    int page_ = 0;
    int documentIndex_ = 0;
    int brush_ = 0;
    int focusedItem_ = -1;
    bool open_ = false;
    bool paused_ = false;
    bool ready_ = false;
    bool frameCaptured_ = false;
    bool observation_ = false;
    bool terrain_ = false;
    bool pauseBeforeObservation_ = false;
    int pickingSlot_ = -1;
    FarmComparisonStatus lastComparisonStatus_ = FarmComparisonStatus::Idle;
};
