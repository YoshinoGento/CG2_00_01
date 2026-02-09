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
#include "SrvManager.h"
#include "ParticleManager.h" 
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	std::unique_ptr<DirectXCommon> dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(winApp.get());

	std::unique_ptr<SrvManager> srvManager = std::make_unique<SrvManager>();
	srvManager->Initialize(dxCommon.get());

	std::unique_ptr<Input> input = std::make_unique<Input>();
	input->Initialize(winApp.get());

	std::unique_ptr<Audio> audio = std::make_unique<Audio>();
	audio->Initialize();

	std::unique_ptr<SpriteCommon> spriteCommon = std::make_unique<SpriteCommon>();
	spriteCommon->Initialize(dxCommon.get(), srvManager.get());

	std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
	object3dCommon->Initialize(dxCommon.get(), srvManager.get());

	std::unique_ptr<ModelManager> modelManager = std::make_unique<ModelManager>();
	modelManager->Initialize(dxCommon.get(), srvManager.get());

	std::unique_ptr<ParticleManager> particleManager = std::make_unique<ParticleManager>();
	particleManager->Initialize(dxCommon.get(), srvManager.get());

	// ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());

	// ImGui用のSRV(描画用メモリ)を1つ確保
	uint32_t imGuiSrvIndex = srvManager->Allocate();

	ImGui_ImplDX12_Init(
		dxCommon->GetDevice(),
		2, // backBuffersの数
		DXGI_FORMAT_R8G8B8A8_UNORM,
		srvManager->GetSrvDescriptorHeap(),
		srvManager->GetCPUDescriptorHandle(imGuiSrvIndex),
		srvManager->GetGPUDescriptorHandle(imGuiSrvIndex)
	);


	// --- リソース読み込み ---
	// テクスチャ
	// "Resources/particle.png" が無ければ "texture.png" など適当なものを入れてください
	uint32_t particleTextureHandle = spriteCommon->LoadTexture("uvChecker.png");
	particleManager->CreateParticleGroup("TestGroup", particleTextureHandle);

	uint32_t textureHandle = spriteCommon->LoadTexture("uvChecker.png");

	// スプライト
	std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon.get(), textureHandle);
	sprite->SetPosition({ 10, 10 });

	// 3Dモデル
	// "Resources/cube.obj" が無ければ作成してください
	modelManager->LoadModel("plane.obj");
	Model* model = modelManager->GetModel("plane.obj");

	
	std::unique_ptr<Object3d> object3d1 = std::make_unique<Object3d>();
	object3d1->Initialize(object3dCommon.get());
	object3d1->SetModel(model);
	object3d1->SetPosition({ -2.0f, 0.0f, 0.0f });

	std::unique_ptr<Object3d> object3d2 = std::make_unique<Object3d>();
	object3d2->Initialize(object3dCommon.get());
	object3d2->SetModel(model);
	object3d2->SetPosition({ 2.0f, 0.0f, 0.0f });

	// カメラ
	std::unique_ptr<Camera> camera = std::make_unique<Camera>();
	Vector3 cameraTranslate = { 0.0f, 2.0f, -10.0f };
	Vector3 cameraRotate = { 0.2f, 0.0f, 0.0f };
	camera->SetTranslate(cameraTranslate);
	camera->SetRotate(cameraRotate);


	while (true) {
		if (winApp->ProcessMessage()) {
			break;
		}

		// --- 更新処理 ---
		input->Update();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (input->TriggerKey(DIK_SPACE)) {
			particleManager->Emit("TestGroup", { 0.0f, 0.0f, 0.0f }, 10);
		}

		// カメラ移動
		float kCameraSpeed = 0.1f;
		if (input->PushKey(DIK_LEFT)) { cameraTranslate.x -= kCameraSpeed; }
		if (input->PushKey(DIK_RIGHT)) { cameraTranslate.x += kCameraSpeed; }
		if (input->PushKey(DIK_UP)) { cameraTranslate.z += kCameraSpeed; }
		if (input->PushKey(DIK_DOWN)) { cameraTranslate.z -= kCameraSpeed; }

		ImGui::Begin("Camera Debug");
		ImGui::DragFloat3("Translate", &cameraTranslate.x, 0.1f);
		ImGui::DragFloat3("Rotate", &cameraRotate.x, 0.01f);
		if (ImGui::Button("Emit Particle")) {
			particleManager->Emit("TestGroup", { 0.0f, 0.0f, 0.0f }, 10);
		}
		ImGui::End();

		camera->SetTranslate(cameraTranslate);
		camera->SetRotate(cameraRotate);
		camera->Update();

		object3d1->SetRotation({ 0.0f, object3d1->GetRotation().y + 0.03f, 0.0f });
		object3d1->Update(camera.get());

		object3d2->SetRotation({ 0.0f, object3d2->GetRotation().y - 0.03f, 0.0f });
		object3d2->Update(camera.get());

		sprite->Update();

		particleManager->Update(camera.get());


		// --- 描画処理 ---
		dxCommon->PreDraw();
		srvManager->PreDraw();

		// 1. 3Dオブジェクト描画
		object3dCommon->CommonDrawSettings();
		object3d1->Draw();
		object3d2->Draw();

		// 2. パーティクル描画 (パーティクル用の設定に切り替わる)
		particleManager->Draw();

		// 3. スプライト描画
		// ★ここが抜けていました！スプライト用の設定（PSO/RootSignature）に戻す必要があります
		spriteCommon->PreDraw();
		sprite->Draw();

		// ImGui描画
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());

		dxCommon->PostDraw();
	}

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	winApp->Finalize();
	return 0;
}