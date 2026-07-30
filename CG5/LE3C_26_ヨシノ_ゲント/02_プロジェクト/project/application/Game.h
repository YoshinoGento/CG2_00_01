#pragma once
#include "base/Framework.h"
#include "math/Matrix.h"	
#include <memory>

class SceneFactory;
class EditorShell;
class PostEffectSystem;
class PostEffectSubmissionDemo;
class PostEffectSubmissionHUD;

/**
 * Gameクラス
 * Application entry point. It owns the main loop, scene flow, and top-level systems.
 */
class Game : public Framework {
public:
	Game();
	~Game() override;

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	// 各シーンから「補正後のマウス座標」を取得するための関数
	static Vector2 GetMousePosInViewport() { return mousePosInViewport_; }

private:
	std::unique_ptr<SceneFactory> sceneFactory_;

	// 計算・補正されたマウス座標を保持
	static Vector2 mousePosInViewport_;

	std::unique_ptr<PostEffectSystem> postEffectSystem_;
	std::unique_ptr<PostEffectSubmissionDemo> postEffectSubmissionDemo_;
	std::unique_ptr<PostEffectSubmissionHUD> postEffectSubmissionHud_;
#ifdef USE_IMGUI
	std::unique_ptr<EditorShell> editorShell_;
#endif
};
