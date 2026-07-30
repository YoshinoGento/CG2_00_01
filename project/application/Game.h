#pragma once
#include "base/Framework.h"
#include "math/Matrix.h"	
#include <memory>

class SceneFactory;
class SkinningDebugWindow;
class EngineDebugWindowManager;
class PostEffectSystem;
class PostEffectDebugWindow;
class CG4EvaluationWindow;

/**
 * Gameクラス
 * プロジェクトのメイン管理者。エディタUI（DockSpace）と
 * ゲーム画面の表示・座標補正を担当します。
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
#ifdef USE_IMGUI
	enum class RuntimeViewMode {
		Debug,
		Play,
	};
#endif

	std::unique_ptr<SceneFactory> sceneFactory_;

	// 計算・補正されたマウス座標を保持
	static Vector2 mousePosInViewport_;

	std::unique_ptr<PostEffectSystem> postEffectSystem_;
#ifdef USE_IMGUI
	std::unique_ptr<SkinningDebugWindow> skinningDebugWindow_;
	std::unique_ptr<EngineDebugWindowManager> engineDebugWindowManager_;
	std::unique_ptr<PostEffectDebugWindow> postEffectDebugWindow_;
	std::unique_ptr<CG4EvaluationWindow> cg4EvaluationWindow_;
	bool showLegacyDebugWindows_ = false;
	RuntimeViewMode runtimeViewMode_ = RuntimeViewMode::Debug;
	bool playModeInitializationPending_ = false;
#endif
};
