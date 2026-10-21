#include "SceneManager.h"
#include "AbstractSceneFactory.h"
#include "2d/TextureManager.h"
#include <cassert>
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "base/Logger.h"
#include "io/Input.h"

SceneManager* SceneManager::instance = nullptr;

SceneManager* SceneManager::GetInstance() {
	if (instance == nullptr) instance = new SceneManager();
	return instance;
}

void SceneManager::DeleteInstance() {
	if (instance) { delete instance; instance = nullptr; }
}

SceneManager::~SceneManager() {
    loading_.Cancel();
	if (scene_) scene_->Finalize();
	// unique_ptr なので手動の delete は不要です
}

/**
 * 更新処理
 * 予約された次のシーン（nextScene_）がある場合、入れ替えを行います。
 */
void SceneManager::BeginFrame() {
    if (loading_.GetStatus() == scene::SceneLoadSystem::Status::Loading) {
        loading_.Advance();
        if (loading_.GetStatus() == scene::SceneLoadSystem::Status::Loading) return;
        const auto& metrics = loading_.GetMetrics();
        Logger::Log("SceneLoad: frames=" + std::to_string(metrics.frames) +
            ", steps=" + std::to_string(metrics.steps) + ", workMs=" + std::to_string(metrics.workMs) +
            ", longestStepMs=" + std::to_string(metrics.longestStepMs) + "\n");
        for (const auto& sample : metrics.slowest) {
            if (sample.index) Logger::Log("SceneLoad step=" + std::to_string(sample.index) +
                ", ms=" + std::to_string(sample.milliseconds) + "\n");
        }
        if (loading_.GetStatus() == scene::SceneLoadSystem::Status::Failed) {
            Logger::Warning("Scene initialization failed; closing before exposing partial resources.");
            PostQuitMessage(1);
            return;
        }
        transition_.BeginReveal();
    }
	if (scene_ && transition_.ConsumeScreenChange()) {
		scene_->OnScreenTransitionCovered();
		transition_.BeginReveal();
	}
	if (nextScene_ && (!transition_.IsActive() || transition_.CanSwap())) {
		if (scene_) {
			scene_->Finalize();
			// PostDraw waits for the previous frame fence, so Update is the safe descriptor/resource release boundary.
			TextureManager::GetInstance()->ReleaseSceneTextures();
		}

		// ★所有権を移動。この代入の瞬間に古い scene_ は自動で破棄されます。
		scene_ = std::move(nextScene_);
		nextScene_ = nullptr;

		scene_->SetSceneManager(this);
        if (transition_.CanSwap()) {
            loading_.Begin(scene_->InitializeIncrementally());
        } else {
            scene_->Initialize();
            transition_.ClearDrops();
        }
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime) {
	if (GetCurrentScene() && !transition_.IsActive()) {
		scene_->FixedUpdate(fixedDeltaTime);
	}
}

void SceneManager::PrepareFixedUpdate() {
	if (GetCurrentScene() && !transition_.IsActive()) {
		scene_->PrepareFixedUpdate();
	}
}

void SceneManager::Update() {
	if (GetCurrentScene()) {
		if (!transition_.IsActive()) scene_->Update();
		else if (transition_.IsCovering()) scene_->UpdateTransitionPresentation();
	}
	const auto* framework = Framework::GetInstance();
	const auto* clock = framework->GetFrameClock();
	const auto* input = framework->GetInput();
	const bool released = input && !input->PushKey(InputKey::Enter) && !input->PushKey(InputKey::Space) &&
		!input->PushKey(InputKey::Escape) && !input->PushMouseButton(InputMouseButton::Left) &&
		!input->PushMouseButton(InputMouseButton::Right) && !input->PushGamepadButton(InputGamepadButton::A) &&
		!input->PushGamepadButton(InputGamepadButton::B);
	transition_.Update(clock ? clock->GetRealDeltaSeconds() : 0,released);
	transitionAudio_.PlayCues(transition_.ConsumeSoundCues());
}

void SceneManager::Draw() {
	if (GetCurrentScene()) {
		scene_->Draw();
	}
	if (transitionRenderer_.Draw(transition_.GetFrame())) transition_.NotifyCoverPresented();
}

void SceneManager::InitializeTransitions(DirectXCommon* dx, SrvManager* srv, Audio* audio) {
	if (!transitionRenderer_.Initialize(dx,srv)) Logger::Warning("Ripple transition unavailable; direct scene change remains possible.");
	if (!transitionAudio_.Initialize(audio)) Logger::Warning("Transition SE unavailable; continuing silently.");
}

bool SceneManager::ChangeSceneWithRipple(const std::string& sceneName, scene::RipplePoint origin,
    const scene::PondRipplePath& pond) {
	if (!sceneFactory_ || nextScene_ || transition_.IsActive()) return false;
	auto next = sceneFactory_->CreateScene(sceneName);
	if (!next) return false;
	nextScene_ = std::move(next);
    if (scene_ && transitionRenderer_.IsReady()) transition_.Begin(origin,
        scene::TransitionTarget::SceneReplacement,scene::TransitionSoundTheme::Water,scene::TransitionStyle::Ripple,pond);
	return true;
}

ScreenTransitionResult SceneManager::BeginScreenTransition(const BaseScene* owner, scene::RipplePoint origin,
    scene::TransitionSoundTheme sound, scene::TransitionStyle style) {
	if (!owner || owner != scene_.get() || nextScene_ || transition_.IsActive()) return ScreenTransitionResult::Busy;
	if (!transitionRenderer_.IsReady()) return ScreenTransitionResult::Unavailable;
	return transition_.Begin(origin, scene::TransitionTarget::ScreenPresentation, sound, style)
		? ScreenTransitionResult::Started : ScreenTransitionResult::Busy;
}

/**
 * 名前指定によるシーン切り替え
 */
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_ && "SceneFactory is not set.");

	// ★工場(CreateScene)が返す unique_ptr を、そのまま予約用関数の引数へ渡します
	SetNextScene(sceneFactory_->CreateScene(sceneName));
}
