#include "GamePlayScene.h"
#include "Framework.h"
#include "Sprite.h"
#include "Object3d.h"
#include "Model.h"
#include "Camera.h"
#include "Audio.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "Game.h"
#include "PrimitiveGenerator.h"
#include <dinput.h>
#include <cmath>
#include <algorithm>

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::Initialize() {
	framework_ = Framework::GetInstance();
	AddLog("Scene: GamePlay Initialized.");

	textureHandles_.clear();
	for (const auto& name : textureNames_) {
		textureHandles_.push_back(framework_->GetSpriteCommon()->LoadTexture("Resources/" + name));
	}

	soundHandles_[0] = framework_->GetAudio()->LoadAudio("Resources/bgm.wav");
	soundHandles_[1] = framework_->GetAudio()->LoadAudio("Resources/Player.mp3");

	framework_->GetModelManager()->LoadModel("plane.obj");
	Model* model = framework_->GetModelManager()->GetModel("plane.obj");
	if (model) { model->LoadTextures(framework_->GetSpriteCommon()); }

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(framework_->GetObject3dCommon());
	object3d_->SetModel(model);

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(framework_->GetSpriteCommon(), textureHandles_[0]);

	camera_ = std::make_unique<Camera>();
	framework_->GetParticleManager()->CreateParticleGroup("Spark", textureHandles_[2]);

	CreateSphere(sphereRadius_);
}

void GamePlayScene::CreateSphere(float radius) {
	sphereModel_ = PrimitiveGenerator::CreateSphere(framework_->GetModelManager(), radius, 32);
	if (!sphereObj_) {
		sphereObj_ = std::make_unique<Object3d>();
		sphereObj_->Initialize(framework_->GetObject3dCommon());
	}
	sphereObj_->SetModel(sphereModel_.get());
	sphereObj_->SetTexture(textureHandles_[1]);
}

void GamePlayScene::Finalize() {}

void GamePlayScene::Update() {
	HandleKeyboardMovement();

	if (!ImGui::GetIO().WantCaptureMouse) {
		Vector2 m = Game::GetMousePosInViewport();
		if (ImGui::IsMouseDown(0)) {
			if (selectedTarget_ == 1) spritePos_ = m;
			else if (selectedTarget_ == 4) {
				spherePos_.x = (m.x - 640.0f) / 50.0f;
				spherePos_.y = -(m.y - 360.0f) / 50.0f;
			}
		}
	}

	sprite_->SetPosition(spritePos_);
	sprite_->Update();
	camera_->Update();

	object3d_->SetPosition(objectPos_);
	object3d_->Update(camera_.get());

	if (sphereObj_) {
		sphereObj_->SetPosition(spherePos_);
		sphereObj_->SetRotation(objectRot_); // ★ImGuiからの回転を適用
		sphereObj_->Update(camera_.get());
	}

	framework_->GetParticleManager()->Update(camera_.get());
}

void GamePlayScene::HandleKeyboardMovement() {
	if (selectedTarget_ == 0 || ImGui::GetIO().WantCaptureKeyboard) return;
	float val = framework_->GetInput()->PushKey(DIK_LSHIFT) ? 10.0f : 1.0f;
	Vector2 d = { 0, 0 };
	if (framework_->GetInput()->PushKey(DIK_UP))    d.y -= val;
	if (framework_->GetInput()->PushKey(DIK_DOWN))  d.y += val;
	if (framework_->GetInput()->PushKey(DIK_LEFT))  d.x -= val;
	if (framework_->GetInput()->PushKey(DIK_RIGHT)) d.x += val;

	if (selectedTarget_ == 1) {
		spritePos_.x += d.x; spritePos_.y += d.y;
	} else if (selectedTarget_ == 4) {
		spherePos_.x += d.x * 0.1f; spherePos_.y -= d.y * 0.1f;
	}
}

void GamePlayScene::Draw() {
	// ★modelPriority_ に基づいて描画順を切り替える
	if (modelPriority_ == 0) {
		// 3D -> 2D (スプライトが手前)
		framework_->GetObject3dCommon()->CommonDrawSettings();
		if (sphereObj_) sphereObj_->Draw();
		framework_->GetSpriteCommon()->PreDraw();
		sprite_->Draw();
	} else {
		// 2D -> 3D (球体が手前)
		framework_->GetSpriteCommon()->PreDraw();
		sprite_->Draw();
		framework_->GetObject3dCommon()->CommonDrawSettings();
		if (sphereObj_) sphereObj_->Draw();
	}

	framework_->GetParticleManager()->Draw();
}

void GamePlayScene::AddLog(const std::string& message) {
	debugLogs_.push_back(message);
	if (debugLogs_.size() > 50) debugLogs_.erase(debugLogs_.begin());
}