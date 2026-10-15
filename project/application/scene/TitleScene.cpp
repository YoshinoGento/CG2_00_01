#include "TitleScene.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "base/Logger.h"
#include "io/Input.h"
#include "SceneManager.h"
#include "title/TitlePresentationLayout.h"

void TitleScene::Initialize() {
    auto* framework = Framework::GetInstance();
    const bool pondLayoutReady = pond_.Initialize();
    const bool worldReady = presentation_.Initialize() && renderer_.Initialize(framework->GetObject3dCommon(),
        framework->GetModelManager(),framework->GetTextureManager(),presentation_.GetFarm(),
        pondLayoutReady ? &pond_.GetLayout() : nullptr);
    ready_ = view_.Initialize(framework->GetSpriteCommon());
    settings_.Initialize();
    pondReady_ = pondLayoutReady && worldReady &&
        pondRenderer_.Initialize(framework->GetDxCommon(),framework->GetSrvManager(),framework->GetTextureManager());
    if (!pondReady_)
        Logger::Warning("Title pond unavailable; start remains accessible with the original ripple.");
    if (!settingsView_.Initialize(framework->GetSpriteCommon())) Logger::Warning("Title settings view unavailable; keyboard controls remain active.");
    if (!audio_.Initialize(framework->GetAudio())) Logger::Warning("Title audio unavailable; continuing without sound.");
    if (!worldReady || !ready_) Logger::Log("TitleScene initialization failed: required title resources unavailable.\n");
    renderer_.Update(presentation_.GetFrame());
    pond_.Update(0,presentation_.GetFrame());
}

void TitleScene::Finalize() {
    settings_.Save(); audio_.Finalize(); pondRenderer_.Finalize(); renderer_.Finalize();
    ready_ = pondReady_ = false;
}

void TitleScene::RequestStart() noexcept {
    if (settings_.GetFrame().open || presentation_.IsLeaving()) return;
    if (pondReady_) pond_.RequestStart();
    presentation_.RequestStart();
}

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
        const bool start = settings_.Update(command);
        if (start) {
            if (pondReady_) pond_.RequestStart(settings_.GetWaterClick());
            presentation_.RequestStart();
        }
    }
    presentation_.Update(clock ? clock->GetRealDeltaSeconds() : 0);
    const auto frame = presentation_.GetFrame();
    pond_.Update(clock ? clock->GetRealDeltaSeconds() : 0,frame);
    renderer_.Update(frame);
    audio_.Update(settings_.Apply(frame.audio));
    // A failed visual asset must not strand the player; start still reaches the game.
    if (presentation_.ConsumeStart() && sceneManager_) {
        const auto& path = pond_.GetStartPath();
        sceneManager_->ChangeSceneWithRipple("GAMEPLAY",path.impact,path);
    }
}

void TitleScene::Draw() {
    renderer_.Draw();
    const auto frame = presentation_.GetFrame();
    if (ready_) view_.DrawLogo(frame);
    if (pondReady_) pondRenderer_.Draw(pond_.GetFrame(),renderer_.GetReflectionSrv());
    if (ready_) view_.DrawPrompt(frame);
    if (!presentation_.IsLeaving()) settingsView_.Draw(settings_.GetFrame());
}
