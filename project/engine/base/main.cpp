#include "Game.h"
#include "debug/ResourceLeakChecker.h"
#include <Windows.h>
#include "base/D3D12DeviceSelection.h"

/**
 * WinMain: プログラムが最初に動き出す場所
 */
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// 1. ゲーム本体（Gameクラス）を作成します。
	// Framework（エンジン層）のポインタで持つことで、共通の Run() 関数を呼び出せるようにしています。
#ifdef _DEBUG
	ResourceLeakChecker resourceLeakChecker;
#endif
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	// 2. ゲームを実行します。
	// この1行の中で「初期化 → ループ（更新・描画） → 終了処理」が自動的に行われます。
	try {
		game->Run();
	} catch (const graphics::InitializationError& error) {
		MessageBoxA(nullptr, error.what(), "CG2 - DirectX12 startup failed", MB_OK | MB_ICONERROR);
		return EXIT_FAILURE;
	}

	// 3. プログラムを正常終了します。
	return 0;
}
