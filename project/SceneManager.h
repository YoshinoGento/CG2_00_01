#pragma once
#include "BaseScene.h"
#include <string>

class AbstractSceneFactory;

/**
 * SceneManagerクラス
 */
class SceneManager {
private:
	static SceneManager* instance;
	BaseScene* scene_ = nullptr;
	BaseScene* nextScene_ = nullptr;
	AbstractSceneFactory* sceneFactory_ = nullptr;

	SceneManager() = default;
	~SceneManager();

public:
	static SceneManager* GetInstance();
	static void DeleteInstance();

	void Update();
	void Draw();

	// ★追加: 現在実行中のシーンを取得（GameクラスのエディタUI用）
	BaseScene* GetCurrentScene() const { return scene_; }

	void SetNextScene(BaseScene* nextScene) {
		nextScene_ = nextScene;
	}

	void ChangeScene(const std::string& sceneName);

	void SetSceneFactory(AbstractSceneFactory* factory) {
		sceneFactory_ = factory;
	}
};