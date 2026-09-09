#pragma once
#include "farm/ui/FarmRuntimeUI.h"

class GamePlayScene;
class Input;

// Routes Release controls to existing gameplay Systems; owns only menu/input state.
class FarmRuntimeController final {
public:
    bool Initialize(SpriteCommon* common) { ready_ = ui_.Initialize(common); return ready_; }
    bool Update(GamePlayScene& scene, const Input& input);
    void Draw() { ui_.Draw(view_, pointer_); }
    bool BlocksSimulation() const noexcept { return open_ || paused_ || frameCaptured_; }
    bool ConsumesInput() const noexcept { return open_ || paused_ || frameCaptured_; }
    bool IsOpen() const noexcept { return open_; }
    void Refresh(GamePlayScene& scene) { BuildView(scene); }
    void OpenLayoutLibrary(GamePlayScene& scene);
private:
    void BuildView(GamePlayScene& scene);
    void Execute(GamePlayScene& scene, farmui::Request request);
    farmui::RuntimeUI ui_;
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
};
