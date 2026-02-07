#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "ModelManager.h"
#include "Model.h"
#include "Input.h"
#include "Audio.h"
#include "Camera.h"
#include <memory>

// ImGuiのヘッダー
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	std::unique_ptr<DirectXCommon> dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(winApp.get());

	std::unique_ptr<Input> input = std::make_unique<Input>();
	input->Initialize(winApp.get());

	std::unique_ptr<Audio> audio = std::make_unique<Audio>();
	audio->Initialize();

	std::unique_ptr<SpriteCommon> spriteCommon = std::make_unique<SpriteCommon>();
	spriteCommon->Initialize(dxCommon.get());

	std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
	object3dCommon->Initialize(dxCommon.get());

	std::unique_ptr<ModelManager> modelManager = std::make_unique<ModelManager>();
	modelManager->Initialize(dxCommon.get());

	// ========================================================================
	// ★★★ ImGuiの初期化 (ここを追加！) ★★★
	// ========================================================================
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());

	// ImGuiが使用するSRVヒープの場所を確保する
	// (SpriteCommonなどで使っている AllocateSRVIndex 関数を利用)
	uint32_t imguiSrvIndex = dxCommon->AllocateSRVIndex();

	ImGui_ImplDX12_Init(
		dxCommon->GetDevice(),
		2, // バックバッファ数 (通常は2)
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, // 画面フォーマット (SRGBで設定)
		dxCommon->GetSrvHeap(),
		dxCommon->GetSRVCPUDescriptorHandle(imguiSrvIndex),
		dxCommon->GetSRVGPUDescriptorHandle(imguiSrvIndex)
	);

	// ========================================================================

	// カメラの生成
	std::unique_ptr<Camera> camera = std::make_unique<Camera>();
	Vector3 cameraTranslate = { 0.0f, 2.0f, -10.0f };
	Vector3 cameraRotate = { 0.1f, 0.0f, 0.0f };
	camera->SetTranslate(cameraTranslate);
	camera->SetRotate(cameraRotate);

	// リソース読み込み
	std::string modelFilename1 = "multiMaterial.obj";
	modelManager->LoadModel(modelFilename1);

	std::string modelFilename2 = "axis.obj";
	modelManager->LoadModel(modelFilename2);

	Model* model1 = modelManager->GetModel(modelFilename1);
	Model* model2 = modelManager->GetModel(modelFilename2);

	if (model1) model1->LoadTextures(spriteCommon.get());
	if (model2) model2->LoadTextures(spriteCommon.get());

	// オブジェクト生成
	std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
	uint32_t spriteTex = spriteCommon->LoadTexture("Resources/uvChecker.png");
	sprite->Initialize(spriteCommon.get(), spriteTex);
	sprite->SetPosition({ 640.0f, 360.0f });

	std::unique_ptr<Object3d> object3d1 = std::make_unique<Object3d>();
	object3d1->Initialize(object3dCommon.get());
	object3d1->SetModel(model1);
	object3d1->SetScale({ 1.0f, 1.0f, 1.0f });
	object3d1->SetPosition({ -2.0f, 0.0f, 0.0f });

	std::unique_ptr<Object3d> object3d2 = std::make_unique<Object3d>();
	object3d2->Initialize(object3dCommon.get());
	object3d2->SetModel(model2);
	object3d2->SetScale({ 1.0f, 1.0f, 1.0f });
	object3d2->SetPosition({ 2.0f, 0.0f, 0.0f });

	while (true) {
		if (winApp->ProcessMessage()) {
			break;
		}

		// ImGuiフレーム開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		input->Update();

		// ImGuiのウィンドウ作成とカメラ操作
		ImGui::Begin("Camera Controller");
		ImGui::DragFloat3("Translation", &cameraTranslate.x, 0.1f);
		ImGui::DragFloat3("Rotation", &cameraRotate.x, 0.01f);
		ImGui::End();

		// カメラに値を反映
		camera->SetTranslate(cameraTranslate);
		camera->SetRotate(cameraRotate);
		camera->Update();

		// オブジェクトの更新
		object3d1->SetRotation({ 0.0f, object3d1->GetRotation().y + 0.03f, 0.0f });
		object3d1->Update(camera.get());

		object3d2->SetRotation({ 0.0f, object3d2->GetRotation().y - 0.03f, 0.0f });
		object3d2->Update(camera.get());

		sprite->Update();

		// 描画開始
		dxCommon->PreDraw();

		// スプライト描画
		spriteCommon->PreDraw();
		sprite->Draw();

		// 3D描画
		object3dCommon->CommonDrawSettings();
		object3d1->Draw();
		object3d2->Draw();

		// ImGuiの描画
		ImGui::Render();
		ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
		ID3D12DescriptorHeap* ppHeaps[] = { dxCommon->GetSrvHeap() };
		commandList->SetDescriptorHeaps(1, ppHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

		// 描画終了
		dxCommon->PostDraw();
	}

	// ★★★ ImGuiの終了処理 (ここも追加！) ★★★
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	audio->Finalize();

	return 0;
}