#pragma once
#include "Framework.h"
#include <memory>

// 前方宣言
class SceneFactory;

/**
 * Gameクラス
 * あなたのプロジェクトのメインクラスです。
 * シーン管理システムを導入し、中身（タイトルやプレイ画面）の切り替えを監督します。
 */
class Game : public Framework {
public:
	Game();
	~Game() override;

	// エンジンの機能をこのゲーム用にセットアップ
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// シーンを作る工場（ファクトリー）を保持
	std::unique_ptr<SceneFactory> sceneFactory_;
};