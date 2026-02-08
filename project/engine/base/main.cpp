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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());

	uint32_t imguiSrvIndex = srvManager->Allocate();
	ImGui_ImplDX12_Init(
		dxCommon->GetDevice(),
		2,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvManager->GetSrvDescriptorHeap(),
		srvManager->GetCPUDescriptorHandle(imguiSrvIndex),
		srvManager->GetGPUDescriptorHandle(imguiSrvIndex)
	);

	std::unique_ptr<Camera> camera = std::make_unique<Camera>();
	Vector3 cameraTranslate = { 0.0f, 2.0f, -10.0f };
	Vector3 cameraRotate = { 0.1f, 0.0f, 0.0f };
	camera->SetTranslate(cameraTranslate);
	camera->SetRotate(cameraRotate);

	// モデル読み込み
	std::string modelFile1 = "axis.obj";
	std::string modelFile2 = "plane.obj";
	modelManager->LoadModel(modelFile1);
	modelManager->LoadModel(modelFile2);

	Model* model1 = modelManager->GetModel(modelFile1);
	Model* model2 = modelManager->GetModel(modelFile2);

	if (model1) model1->LoadTextures(spriteCommon.get());
	if (model2) model2->LoadTextures(spriteCommon.get());

	uint32_t uvCheckerHandle = spriteCommon->LoadTexture("Resources/uvChecker.png");
	std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon.get(), uvCheckerHandle);
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

	// ★ BGM再生
	// Resourcesフォルダに bgm.wav を置いてください
	uint32_t bgmHandle = audio->LoadWave("bgm.wav");
	audio->PlayWave(bgmHandle, true, 0.1f); // ループ再生(true), 音量(0.1)

	while (true) {
		if (winApp->ProcessMessage()) {
			break;
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		input->Update();

		// ★ キー操作でカメラ移動
		float kCameraSpeed = 0.1f;
		if (input->PushKey(DIK_LEFT)) { cameraTranslate.x -= kCameraSpeed; }
		if (input->PushKey(DIK_RIGHT)) { cameraTranslate.x += kCameraSpeed; }
		if (input->PushKey(DIK_UP)) { cameraTranslate.z += kCameraSpeed; }
		if (input->PushKey(DIK_DOWN)) { cameraTranslate.z -= kCameraSpeed; }

		ImGui::Begin("Camera Debug");
		ImGui::DragFloat3("Translate", &cameraTranslate.x, 0.1f);
		ImGui::DragFloat3("Rotate", &cameraRotate.x, 0.01f);
		ImGui::End();

		camera->SetTranslate(cameraTranslate);
		camera->SetRotate(cameraRotate);
		camera->Update();

		object3d1->SetRotation({ 0.0f, object3d1->GetRotation().y + 0.03f, 0.0f });
		object3d1->Update(camera.get());

		object3d2->SetRotation({ 0.0f, object3d2->GetRotation().y - 0.03f, 0.0f });
		object3d2->Update(camera.get());

		sprite->Update();

		dxCommon->PreDraw();

		srvManager->PreDraw();

		spriteCommon->PreDraw();
		sprite->Draw();

		object3dCommon->CommonDrawSettings();
		object3d1->Draw();
		object3d2->Draw();

		ImGui::Render();
		ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
		ID3D12DescriptorHeap* ppHeaps[] = { srvManager->GetSrvDescriptorHeap() };
		commandList->SetDescriptorHeaps(1, ppHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

		dxCommon->PostDraw();
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	audio->Finalize();

	return 0;
}