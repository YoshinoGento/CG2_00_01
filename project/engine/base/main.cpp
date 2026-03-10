#include "Game.h"
#include <Windows.h>

/**
 * WinMain: Windowsアプリの入り口
 * 注釈（_In_など）を付けることで警告C28251を解消します。
 */
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    // 未使用引数の警告避け
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nShowCmd;

    // 1. ゲーム（アプリ層）をエンジン（Framework層）のポインタで作成
    // これにより「Frameworkとして実行するが、中身はGame」というポリモーフィズムを実現
    std::unique_ptr<Framework> game = std::make_unique<Game>();

    // 2. 実行（この1行で初期化・ループ・終了まで完結）
    game->Run();

    return 0;
}