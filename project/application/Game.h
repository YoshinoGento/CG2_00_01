#pragma once
#include "Framework.h"
#include "Matrix.h"	
#include <memory>

class SceneFactory;

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
	std::unique_ptr<SceneFactory> sceneFactory_;

	// ゲーム画面表示用のディスクリプタ番号
	uint32_t viewportSrvIndex_ = 0;

	// 計算・補正されたマウス座標を保持
	static Vector2 mousePosInViewport_;
};