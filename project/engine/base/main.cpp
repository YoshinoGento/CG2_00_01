#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "Object3dCommon.h" // ★追加
#include "Object3d.h"       // ★追加
#include "Input.h"
#include <memory>

// Windowsアプリのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // ========================================================================
    // 1. 基盤システムの初期化
    // ========================================================================

    std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    std::unique_ptr<DirectXCommon> dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

    std::unique_ptr<Input> input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    std::unique_ptr<SpriteCommon> spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->Initialize(dxCommon.get());

    // ★ 3Dオブジェクト共通部の初期化
    std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->Initialize(dxCommon.get());

    // ========================================================================
    // 2. リソース（テクスチャ）の読み込み
    // ========================================================================

    uint32_t textureHandle = spriteCommon->LoadTexture("Resources/uvChecker.png");

    // ========================================================================
    // 3. オブジェクトの生成
    // ========================================================================

    // スプライト生成
    std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), textureHandle);
    sprite->SetPosition({ 640.0f, 360.0f });

    // ★ 3Dオブジェクト生成
    std::unique_ptr<Object3d> object3d = std::make_unique<Object3d>();
    // 1. 初期化（共通部だけ渡す）
    object3d->Initialize(object3dCommon.get());

    // 2. モデルをセット（これを忘れると描画されません）
    object3d->SetModel("Triangle");

    // 3. テクスチャをセット（必要な場合）
    object3d->SetTexture(textureHandle);

    // ========================================================================
    // 4. メインループ
    // ========================================================================

    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        // --- 更新処理 (Update) ---
        input->Update();

        // 3Dオブジェクトの更新 (回転させてみる)
        object3d->SetRotation({ 0.0f, object3d->GetRotation().y + 0.01f, 0.0f });
        object3d->Update();

        // スプライトの更新
        sprite->Update();

        // --- 描画処理 (Draw) ---
        dxCommon->PreDraw();

        // 1. スプライト描画
        spriteCommon->PreDraw();
        sprite->Draw();

        // 2. ★ 3Dオブジェクト描画
        // 共通設定をセットしてから、個別のDrawを呼ぶ
        object3dCommon->CommonDrawSettings();
        object3d->Draw();

        dxCommon->PostDraw();
    }

    return 0;
}