#pragma once

// 前方宣言
class SceneManager;

/**
 * BaseSceneクラス
 * すべてのシーンの親クラスです（スライド25,26準拠）。
 */
class BaseScene {
protected:
	// シーンマネージャを借りてくる（前方宣言を利用）
	SceneManager* sceneManager_ = nullptr;

public:
	virtual ~BaseScene() = default;

	// 各シーンで必ず実装する関数
	virtual void Initialize() = 0;
	virtual void Finalize() = 0;
	virtual void PrepareFixedUpdate() {}
	virtual void FixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
	virtual void Update() = 0;
	virtual void Draw() = 0;

	// シーンマネージャをセットするSetter（スライド25準拠）
	virtual void SetSceneManager(SceneManager* sceneManager) {
		sceneManager_ = sceneManager;
	}
};
