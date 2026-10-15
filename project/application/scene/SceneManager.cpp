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
	if (scene_) scene_->Finalize();
	// unique_ptr なので手動の delete は不要です
}

/**
 * 更新処理
 * 予約された次のシーン（nextScene_）がある場合、入れ替えを行います。
 */
void SceneManager::BeginFrame() {
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
		scene_->Initialize();
		if (transition_.CanSwap()) transition_.BeginReveal();
		else transition_.ClearDrops();
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime) {
	if (scene_ && !transition_.IsActive()) {
		scene_->FixedUpdate(fixedDeltaTime);
	}
}

void SceneManager::PrepareFixedUpdate() {
	if (scene_ && !transition_.IsActive()) {
		scene_->PrepareFixedUpdate();
	}
}

void SceneManager::Update() {
	if (scene_) {
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
	if (scene_) {
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
