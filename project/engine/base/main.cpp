#include "Game.h"
#include <Windows.h>

/**
 * WinMain: プログラムが最初に動き出す場所
 */
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// 1. ゲーム本体（Gameクラス）を作成します。
	// Framework（エンジン層）のポインタで持つことで、共通の Run() 関数を呼び出せるようにしています。
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	// 2. ゲームを実行します。
	// この1行の中で「初期化 → ループ（更新・描画） → 終了処理」が自動的に行われます。
	game->Run();

	// 3. プログラムを正常終了します。
	return 0;
}