#include "TitleScene.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "base/Logger.h"
#include "io/Input.h"
#include "SceneManager.h"

void TitleScene::Initialize() {
    auto* framework = Framework::GetInstance();
    const bool worldReady = presentation_.Initialize() && renderer_.Initialize(framework->GetObject3dCommon(),
        framework->GetModelManager(),framework->GetTextureManager(),presentation_.GetFarm());
    ready_ = view_.Initialize(framework->GetSpriteCommon());
    settings_.Initialize();
    if (!settingsView_.Initialize(framework->GetSpriteCommon())) Logger::Warning("Title settings view unavailable; keyboard controls remain active.");
    if (!audio_.Initialize(framework->GetAudio())) Logger::Warning("Title audio unavailable; continuing without sound.");
    if (!worldReady || !ready_) Logger::Log("TitleScene initialization failed: required title resources unavailable.\n");
    renderer_.Update(presentation_.GetFrame());
}

void TitleScene::Finalize() { settings_.Save(); audio_.Finalize(); renderer_.Finalize(); ready_ = false; }

void TitleScene::Update() {
    auto* framework = Framework::GetInstance();
    const auto* input = framework->GetInput();
    const auto* window = framework->GetWinApp();
    const auto* clock = framework->GetFrameClock();
    if (input && window && !presentation_.IsLeaving()) {
        auto command = settingsView_.ReadInput(*input,*window);
#ifdef USE_IMGUI
        if (!viewportFocused_) command = {};
        command.pointer = viewportInput_.pointer;
        command.pointerValid = viewportInput_.pointerValid;
        command.pressed = viewportInput_.pressed;
        command.held = viewportInput_.held;
        viewportInput_ = {};
#endif
        if (settings_.Update(command)) presentation_.RequestStart();
    }
    presentation_.Update(clock ? clock->GetRealDeltaSeconds() : 0);
    const auto frame = presentation_.GetFrame();
    renderer_.Update(frame);
    audio_.Update(settings_.Apply(frame.audio));
    // A failed visual asset must not strand the player; start still reaches the game.
    if (presentation_.ConsumeStart() && sceneManager_) sceneManager_->ChangeScene("GAMEPLAY");
}

void TitleScene::Draw() {
    renderer_.Draw();
    if (ready_) view_.Draw(presentation_.GetFrame());
    if (!presentation_.IsLeaving()) settingsView_.Draw(settings_.GetFrame());
}
