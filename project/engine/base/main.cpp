#include "Game.h"

/**
 * WinMain
 * アプリケーションのエントリーポイントです。
 * ここでは Game クラスの生成とメインループの実行のみを行います。
 */
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	// 1. ゲームクラスのインスタンスを生成
	// すべてのマネージャやリソースはこの中で管理されます
	std::unique_ptr<Game> game = std::make_unique<Game>();

	// 2. 初期化処理
	// ウィンドウ作成、DirectX、オーディオ、各種マネージャのセットアップが行われます
	game->Initialize();

	// --- 3. メインループ ---
	// ウィンドウが閉じられるまで処理を繰り返します
	while (true) {

		// 更新処理（メッセージ処理、入力、ゲームロジック、ImGuiの計算）
		game->Update();

		// ゲーム側から終了リクエスト（ウィンドウの×ボタンなど）があればループを抜ける
		if (game->IsEndRequest()) {
			break;
		}

		// 描画処理（GPUへのコマンド発行、画面の入れ替え）
		game->Draw();
	}

	// 4. 終了処理
	// メモリの解放や各システムのシャットダウンを行います
	game->Finalize();

	return 0;
}