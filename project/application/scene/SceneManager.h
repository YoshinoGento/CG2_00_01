#pragma once
#include "BaseScene.h"
#include <string>
#include <memory>
#include "SceneTransitionSystem.h"
#include "RippleTransitionRenderer.h"
#include "TransitionAudioSystem.h"
#include "SceneLoadSystem.h"

class AbstractSceneFactory;
enum class ScreenTransitionResult { Started, Unavailable, Busy };

/**
 * SceneManagerクラス
 * シーンの実行と切り替えを安全に管理します。
 */
class SceneManager {
private:
	static SceneManager* instance;

	// ★生のポインタを unique_ptr に変更。これで自動的に delete されるようになります。
	std::unique_ptr<BaseScene> scene_;
	std::unique_ptr<BaseScene> nextScene_;
	AbstractSceneFactory* sceneFactory_ = nullptr;
	scene::SceneTransitionSystem transition_;
	scene::RippleTransitionRenderer transitionRenderer_;
	scene::TransitionAudioSystem transitionAudio_;
    scene::SceneLoadSystem loading_;

	SceneManager() = default;
	~SceneManager();

public:
	static SceneManager* GetInstance();
	static void DeleteInstance();

	void Update();
	void BeginFrame();
	void PrepareFixedUpdate();
	void FixedUpdate(float fixedDeltaTime);
	void Draw();
	void InitializeTransitions(DirectXCommon* dx, SrvManager* srv, Audio* audio);
    bool ChangeSceneWithRipple(const std::string& sceneName, scene::RipplePoint origin = {},
        const scene::PondRipplePath& pond = {});
	ScreenTransitionResult BeginScreenTransition(const BaseScene* owner, scene::RipplePoint origin = {},
		scene::TransitionSoundTheme sound = scene::TransitionSoundTheme::Water,
		scene::TransitionStyle style = scene::TransitionStyle::Ripple);
	void PreviewWaterDrop(scene::RipplePoint origin) { transition_.AddDrop(origin); }
    [[nodiscard]] bool IsTransitionCoverPresented() const noexcept { return transition_.CanSwap(); }

	// ★追加: 現在実行中のシーンを取得する（GameクラスでのUI表示に必要）
	BaseScene* GetCurrentScene() const { return loading_.BlocksScene() ? nullptr : scene_.get(); }

	// ★引数を unique_ptr にし、所有権を受け取る（std::move）ように変更
	void SetNextScene(std::unique_ptr<BaseScene> nextScene) {
		if (!transition_.IsActive()) nextScene_ = std::move(nextScene);
	}

	void ChangeScene(const std::string& sceneName);

	void SetSceneFactory(AbstractSceneFactory* factory) {
		sceneFactory_ = factory;
	}
};
