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
#include <assert.h>

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

	// ⚠️ 以前ここにあった ImGui の初期化は、下（画像読み込みの後）に移動しました！


	// --- リソース読み込み ---
	uint32_t particleTextureHandle = spriteCommon->LoadTexture("uvChecker.png");
	particleManager->CreateParticleGroup("TestGroup", particleTextureHandle);

	uint32_t textureHandle = spriteCommon->LoadTexture("uvChecker.png");

	// スプライト
	std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon.get(), textureHandle);
	sprite->SetPosition({ 10, 10 });

	// 3Dモデル
	modelManager->LoadModel("plane.obj");
	Model* model = modelManager->GetModel("plane.obj");

	if (model) {
		model->LoadTextures(spriteCommon.get());
		const std::string& modelTexPath = model->GetTextureFilePath();
		if (!modelTexPath.empty()) {
			uint32_t modelTexHandle = spriteCommon->LoadTexture(modelTexPath);
		}
	}

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


	// ==========================================================
	// ★ 【重要】ImGuiの初期化をここ（ループの直前）に移動！
	// ==========================================================
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	if (!ImGui_ImplWin32_Init(winApp->GetHwnd())) {
		assert(false && "ImGui Win32の初期化に失敗しました");
	}

	uint32_t imGuiSrvIndex = srvManager->Allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvManager->GetCPUDescriptorHandle(imGuiSrvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = srvManager->GetGPUDescriptorHandle(imGuiSrvIndex);

	if (!ImGui_ImplDX12_Init(
		dxCommon->GetDevice(),
		2, // backBuffersの数
		DXGI_FORMAT_R8G8B8A8_UNORM,
		srvManager->GetSrvDescriptorHeap(), // 最新のマンションの住所を渡す！
		cpuHandle,
		gpuHandle
	)) {
		assert(false && "ImGui DX12の初期化に失敗しました。");
	}

	// 🚨【最重要の追加】フォントデータが作られないエラー（TexIsBuilt）をこれで防ぎます！
	ImGui::GetIO().Fonts->Build();
	ImGui_ImplDX12_CreateDeviceObjects();
	// ==========================================================


	// --- メインループ ---
	while (true) {
		if (winApp->ProcessMessage()) {
			break;
		}

		input->Update();

		// スペースキーでパーティクル発生
		if (input->TriggerKey(DIK_SPACE)) {
			particleManager->Emit("TestGroup", { 0.0f, 0.0f, 0.0f }, 10);
		}

		// ImGuiフレーム開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// デバッグUIの表示
		ImGui::Begin("Debug Window");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::DragFloat3("Camera Pos", &cameraTranslate.x, 0.1f);
		if (ImGui::Button("Reset Camera")) {
			cameraTranslate = { 0.0f, 2.0f, -10.0f };
		}
		// ImGuiのボタンでパーティクル発生
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

		// 1. 3D描画
		object3dCommon->CommonDrawSettings();
		object3d1->Draw();
		object3d2->Draw();

		// 2. パーティクル描画
		particleManager->Draw();

		// 3. スプライト描画
		spriteCommon->PreDraw();
		sprite->Draw();

		// 4. ImGui描画
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());

		dxCommon->PostDraw();
	}

	// 終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	winApp->Finalize();
	return 0;
}