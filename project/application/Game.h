#pragma once
#include "base/Framework.h"
#include "math/Matrix.h"	
#include <memory>

class SceneFactory;
class SkinningDebugWindow;
class DebugEditorWindow;
class EngineDebugWindowManager;
class PostEffectSystem;
class PostEffectDebugWindow;

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
	enum class DebugUiLanguage {
		Japanese,
		English,
	};

	void DrawDebugMasterTopBar(class GamePlayScene* playScene);
	void DrawDebugEditor(class GamePlayScene* playScene);
	const char* DebugLabel(const char* japanese, const char* english) const;
	void LoadDebugUiSettings();
	void SaveDebugUiSettings() const;
	bool ShouldDrawLegacyDebugWindows() const;
	bool ShouldDrawSceneDebugWindows() const;
	bool ShouldDrawFarmDebugWindows() const;
#endif

	std::unique_ptr<SceneFactory> sceneFactory_;

	// 計算・補正されたマウス座標を保持
	static Vector2 mousePosInViewport_;

	std::unique_ptr<PostEffectSystem> postEffectSystem_;
#ifdef USE_IMGUI
	std::unique_ptr<DebugEditorWindow> debugEditorWindow_;
	std::unique_ptr<SkinningDebugWindow> skinningDebugWindow_;
	std::unique_ptr<EngineDebugWindowManager> engineDebugWindowManager_;
	std::unique_ptr<PostEffectDebugWindow> postEffectDebugWindow_;
	DebugUiLanguage debugUiLanguage_ = DebugUiLanguage::Japanese;
#endif
};
