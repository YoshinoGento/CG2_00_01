#pragma once
#include "BaseScene.h"
#include <string>
#include <memory>

class AbstractSceneFactory;

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

	// ★追加: 現在実行中のシーンを取得する（GameクラスでのUI表示に必要）
	BaseScene* GetCurrentScene() const { return scene_.get(); }

	// ★引数を unique_ptr にし、所有権を受け取る（std::move）ように変更
	void SetNextScene(std::unique_ptr<BaseScene> nextScene) {
		nextScene_ = std::move(nextScene);
	}

	void ChangeScene(const std::string& sceneName);

	void SetSceneFactory(AbstractSceneFactory* factory) {
		sceneFactory_ = factory;
	}
};
