#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "ModelCommon.h"
#include "Model.h"
#include "Input.h"
#include <memory>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // 1. 基盤システムの初期化
    std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    std::unique_ptr<DirectXCommon> dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

    std::unique_ptr<Input> input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    std::unique_ptr<SpriteCommon> spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->Initialize(dxCommon.get());

    std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->Initialize(dxCommon.get());

    std::unique_ptr<ModelCommon> modelCommon = std::make_unique<ModelCommon>();
    modelCommon->Initialize(dxCommon.get());

    // 2. リソース読み込み
    // モデル読み込み (データは1回読むだけでOK！)
    modelCommon->LoadModel("plane.obj");
    Model* model = modelCommon->GetModel("plane.obj");

    // テクスチャ読み込み
    uint32_t textureHandle = 0;
    if (model) {
        textureHandle = spriteCommon->LoadTexture(model->GetTextureFilePath());
    }

    // 3. オブジェクト生成
    std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), textureHandle);
    sprite->SetPosition({ 640.0f, 360.0f });

    // ★ 1つ目の3Dオブジェクト
    std::unique_ptr<Object3d> object3d1 = std::make_unique<Object3d>();
    object3d1->Initialize(object3dCommon.get());
    object3d1->SetModel(model); // 共通のモデルをセット
    object3d1->SetTexture(textureHandle);
    object3d1->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d1->SetPosition({ -2.0f, 0.0f, 10.0f }); // 左側に配置
    object3d1->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // ★ 2つ目の3Dオブジェクト (ここを追加！)
    std::unique_ptr<Object3d> object3d2 = std::make_unique<Object3d>();
    object3d2->Initialize(object3dCommon.get());
    object3d2->SetModel(model); // 1つ目と同じモデルを使い回せる！
    object3d2->SetTexture(textureHandle);
    object3d2->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d2->SetPosition({ 2.0f, 0.0f, 10.0f }); // 右側に配置
    object3d2->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // 4. メインループ
    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        input->Update();

        // 1つ目の更新 (右回転)
        object3d1->SetRotation({ 0.0f, object3d1->GetRotation().y + 0.03f, 0.0f });
        object3d1->Update();

        // 2つ目の更新 (左回転させてみる)
        object3d2->SetRotation({ 0.0f, object3d2->GetRotation().y - 0.03f, 0.0f });
        object3d2->Update();

        sprite->Update();

        // 描画
        dxCommon->PreDraw();

        spriteCommon->PreDraw();
        sprite->Draw();

        // 3D描画
        object3dCommon->CommonDrawSettings(); // 共通設定は1回でOK

        object3d1->Draw(); // 1つ目を描画
        object3d2->Draw(); // 2つ目を描画

        dxCommon->PostDraw();
    }

    return 0;
}