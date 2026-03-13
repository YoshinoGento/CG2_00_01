#pragma once
#include "BaseScene.h"
#include <string>

// シーンファクトリーのインターフェース（エラー解消用）
class AbstractSceneFactory;

/**
 * SceneManagerクラス
 * スライドの設計にChangeScene機能等を追加したシングルトンマネージャです。
 */
class SceneManager {
private:
	static SceneManager* instance;
	BaseScene* scene_ = nullptr;
	BaseScene* nextScene_ = nullptr;
	AbstractSceneFactory* sceneFactory_ = nullptr; // エラー解消用

	SceneManager() = default;
	~SceneManager();

public:
	static SceneManager* GetInstance();
	static void DeleteInstance();

	// 基本サイクル
	void Update();
	void Draw();

	// --- エラーを修正するための関数群 ---

	// 次のシーンをインスタンスで予約（スライド11準拠）
	void SetNextScene(BaseScene* nextScene) {
		nextScene_ = nextScene;
	}

	// 次のシーンを名前で予約（エラーC2039解消用）
	void ChangeScene(const std::string& sceneName);

	// シーンファクトリーをセット（エラーC2039解消用）
	void SetSceneFactory(AbstractSceneFactory* factory) {
		sceneFactory_ = factory;
	}
};