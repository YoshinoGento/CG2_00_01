#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "Object3dCommon.h"
#include "Object3d.h"
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

    // 3Dオブジェクト共通部の初期化
    std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->Initialize(dxCommon.get());

    // ========================================================================
    // 2. リソース（テクスチャ・モデル）の読み込み
    // ========================================================================

    uint32_t textureHandle = spriteCommon->LoadTexture("Resources/uvChecker.png");

    // モデル読み込み
    object3dCommon->LoadModel("plane.obj");

    // ========================================================================
    // 3. オブジェクトの生成
    // ========================================================================

    // スプライト生成
    std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), textureHandle);
    sprite->SetPosition({ 640.0f, 360.0f });

    // 3Dオブジェクト生成
    std::unique_ptr<Object3d> object3d = std::make_unique<Object3d>();
    object3d->Initialize(object3dCommon.get());
    object3d->SetModel("plane.obj");
    object3d->SetTexture(textureHandle);

    object3d->SetScale({ 1.0f, 1.0f, 1.0f });

    // ★ 修正1: 遠ざける (Z = 2.0f -> 10.0f くらいにしてみる)
    object3d->SetPosition({ 0.0f, 0.0f, 10.0f });

    // ★ 修正2: アルファ値を上げて透明度を下げる (不透明にする)
    // RGBA = (赤, 緑, 青, アルファ)
    // アルファ値: 0.0(透明) ～ 1.0(不透明)
    // 「透明度を下げる」ということは「不透明にする」ということなので、1.0f に設定します。
    // もし半透明にしたい場合は 0.5f などにしてください。
    object3d->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // ========================================================================
    // 4. メインループ
    // ========================================================================

    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        // --- 更新処理 (Update) ---
        input->Update();

        // 3Dオブジェクトの更新 (回転)
        object3d->SetRotation({ 0.0f, object3d->GetRotation().y + 0.03f, 0.0f });
        object3d->Update();

        // スプライトの更新
        sprite->Update();

        // --- 描画処理 (Draw) ---
        dxCommon->PreDraw();

        // 1. スプライト描画
        spriteCommon->PreDraw();
        sprite->Draw();

        // 2. 3Dオブジェクト描画
        object3dCommon->CommonDrawSettings();
        object3d->Draw();

        dxCommon->PostDraw();
    }

    return 0;
}