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
    if (!worldReady || !ready_) Logger::Log("TitleScene initialization failed: required title resources unavailable.\n");
    renderer_.Update(presentation_.GetFrame());
}

void TitleScene::Finalize() { renderer_.Finalize(); ready_ = false; }

void TitleScene::Update() {
    auto* framework = Framework::GetInstance();
    const auto* input = framework->GetInput();
    const auto* window = framework->GetWinApp();
    const auto* clock = framework->GetFrameClock();
    if (input && window && view_.IsStartRequested(*input,*window)) presentation_.RequestStart();
    presentation_.Update(clock ? clock->GetRealDeltaSeconds() : 0);
    renderer_.Update(presentation_.GetFrame());
    // A failed visual asset must not strand the player; start still reaches the game.
    if (presentation_.ConsumeStart() && sceneManager_) sceneManager_->ChangeScene("GAMEPLAY");
}

void TitleScene::Draw() {
    renderer_.Draw();
    if (ready_) view_.Draw(presentation_.GetFrame());
}
