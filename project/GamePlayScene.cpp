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

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::Initialize() {
	framework_ = Framework::GetInstance();
	AddLog("Scene: GamePlay Initialized.");

	textureHandles_.clear();
	std::vector<std::string> textureNames = { "monsterBall.png", "uvChecker.png", "choju8_0008.png" };
	for (const auto& name : textureNames) {
		textureHandles_.push_back(framework_->GetSpriteCommon()->LoadTexture("Resources/" + name));
	}

	camera_ = std::make_unique<Camera>();

	std::string terrainPath = "terrain/terrain.obj";
	framework_->GetModelManager()->LoadModel(terrainPath);
	Model* tModel = framework_->GetModelManager()->GetModel(terrainPath);
	if (tModel) { tModel->LoadTextures(framework_->GetSpriteCommon()); }

	terrainObj_ = std::make_unique<Object3d>();
	terrainObj_->Initialize(framework_->GetObject3dCommon());
	terrainObj_->SetModel(tModel);
	terrainObj_->SetPosition({ 0.0f, -2.0f, 0.0f });
	terrainObj_->SetScale({ 2.0f, 2.0f, 2.0f });

	framework_->GetModelManager()->LoadModel("plane.obj");
	Model* planeModel = framework_->GetModelManager()->GetModel("plane.obj");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(framework_->GetObject3dCommon());
	object3d_->SetModel(planeModel);

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(framework_->GetSpriteCommon(), textureHandles_[0]);

	framework_->GetParticleManager()->CreateParticleGroup("Spark", textureHandles_[2]);

	CreateSphere(sphereRadius_);
}

void GamePlayScene::Update() {
	HandleKeyboardMovement();

	// カメラの反映
	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);
	camera_->Update();

	sprite_->SetPosition(spritePos_);
	sprite_->Update();

	// --- 各オブジェクトに ImGui の設定を反映させる ---

	// 地面の更新
	if (terrainObj_) {
		terrainObj_->SetCullMode(cullMode_); // ★ここ：カリングを反映
		terrainObj_->SetLightDirection(lightDirection_);
		terrainObj_->SetLightColor({ lightColor_.x, lightColor_.y, lightColor_.z, 1.0f });
		terrainObj_->SetLightIntensity(lightIntensity_);
		terrainObj_->Update(camera_.get());
	}

	// 球体の更新
	if (sphereObj_) {
		sphereObj_->SetCullMode(cullMode_); // ★ここ：カリングを反映
		sphereObj_->SetPosition(spherePos_);
		sphereObj_->SetRotation(objectRot_);
		sphereObj_->SetLightDirection(lightDirection_);
		sphereObj_->SetLightColor({ lightColor_.x, lightColor_.y, lightColor_.z, 1.0f });
		sphereObj_->SetLightIntensity(lightIntensity_);
		sphereObj_->Update(camera_.get());
	}

	// 平面の更新
	if (object3d_) {
		object3d_->SetCullMode(cullMode_); // ★ここ：カリングを反映
		object3d_->SetPosition(objectPos_);
		object3d_->SetLightDirection(lightDirection_);
		object3d_->SetLightColor({ lightColor_.x, lightColor_.y, lightColor_.z, 1.0f });
		object3d_->SetLightIntensity(lightIntensity_);
		object3d_->Update(camera_.get());
	}

	framework_->GetParticleManager()->Update(camera_.get());
}


void GamePlayScene::CreateSphere(float radius) {
	sphereModel_ = PrimitiveGenerator::CreateSphere(framework_->GetModelManager(), radius, 32);
	if (!sphereObj_) {
		sphereObj_ = std::make_unique<Object3d>();
		sphereObj_->Initialize(framework_->GetObject3dCommon());
	}
	sphereObj_->SetModel(sphereModel_.get());
	sphereObj_->SetTexture(textureHandles_[1]);
	sphereObj_->SetShininess(40.0f);
}

void GamePlayScene::Draw() {
	auto objCommon = framework_->GetObject3dCommon();
	auto spriteCommon = framework_->GetSpriteCommon();

	if (modelPriority_ == 0) { // 3D -> 2D
		objCommon->CommonDrawSettings();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();
	} else { // 2D -> 3D
		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();
		objCommon->CommonDrawSettings();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
	}
	if (showParticles_) framework_->GetParticleManager()->Draw();
}

void GamePlayScene::HandleKeyboardMovement() {
	if (selectedTarget_ == 0 || ImGui::GetIO().WantCaptureKeyboard) return;
	Input* input = framework_->GetInput();
	float speed = input->PushKey(DIK_LSHIFT) ? 5.0f : 0.5f;
	Vector3 move = { 0, 0, 0 };
	if (input->PushKey(DIK_UP))    move.y += speed;
	if (input->PushKey(DIK_DOWN))  move.y -= speed;
	if (input->PushKey(DIK_LEFT))  move.x -= speed;
	if (input->PushKey(DIK_RIGHT)) move.x += speed;

	switch (selectedTarget_) {
	case 1: spritePos_.x += move.x; spritePos_.y -= move.y; break;
	case 2: objectPos_.x += move.x * 0.1f; objectPos_.y += move.y * 0.1f; break;
	case 4: spherePos_.x += move.x * 0.1f; spherePos_.y += move.y * 0.1f; break;
	}
}

void GamePlayScene::AddLog(const std::string& message) {
	debugLogs_.push_back(message);
	if (debugLogs_.size() > 50) debugLogs_.erase(debugLogs_.begin());
}

void GamePlayScene::Finalize() {}