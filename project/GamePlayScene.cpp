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
#include <dinput.h>
#include <cmath>
#include <algorithm>

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::Initialize() {
	framework_ = Framework::GetInstance();
	AddLog("Scene: GamePlay Initialized.");

	// アセットの一括読み込み（提供されたリスト）
	textureNames_ = {
		"monsterBall.png", "uvChecker.png", "choju8_0008.png",
		"IMG_0264.jpg", "仏顔.jpg", "魚パン.png"
	};
	textureHandles_.clear();
	for (const auto& name : textureNames_) {
		textureHandles_.push_back(framework_->GetSpriteCommon()->LoadTexture("Resources/" + name));
	}

	// 音声・3Dモデル・スプライトの準備
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

	// 初期設定
	spritePos_ = { 640.0f, 360.0f };
	isSnap_ = true;
	snapStep_ = 32.0f;
	selectedTarget_ = 1;
}

void GamePlayScene::Finalize() {}

void GamePlayScene::Update() {
	frameRates_[frameRateIndex_] = ImGui::GetIO().Framerate;
	frameRateIndex_ = (frameRateIndex_ + 1) % 60;

	// キーボード移動
	HandleKeyboardMovement();

	// スペースキーでパーティクル放出
	if (framework_->GetInput()->TriggerKey(DIK_SPACE)) {
		Vector3 pos = { particleEmitPos_[0], particleEmitPos_[1], particleEmitPos_[2] };
		framework_->GetParticleManager()->Emit("Spark", pos, particleEmitCount_);
		AddLog("Action: Particle Emitted.");
	}

	// ★修正：ImGuiManager::Begin/End は Game.cpp で行われるためここでは不要です
#ifdef USE_IMGUI

	// --- 【ビデオ再現】ウィンドウスナップ ---
	auto ApplyWindowSnap = [&](const char* windowName) {
		if (!isSnap_) return;
		ImVec2 pos = ImGui::GetWindowPos(); ImVec2 size = ImGui::GetWindowSize();
		ImVec2 snapped = { std::round(pos.x / snapStep_) * snapStep_, std::round(pos.y / snapStep_) * snapStep_ };
		if (pos.x < 15) snapped.x = 0; if (pos.y < 15) snapped.y = 0;
		if (pos.x + size.x > 1265) snapped.x = 1280 - size.x;
		if (pos.y + size.y > 705) snapped.y = 720 - size.y;
		if (pos.x != snapped.x || pos.y != snapped.y) ImGui::SetWindowPos(windowName, snapped);
		};

	ImGui::Begin("Global Settings");
	ApplyWindowSnap("Global Settings");
	ImGui::Checkbox("Enable Snap", &isSnap_);
	ImGui::DragFloat("Step", &snapStep_, 1.0f, 4.0f, 128.0f, "%.0f px");
	const char* targets[] = { "None", "Sprite", "Object3D", "Particle" };
	ImGui::Combo("Edit Focus", &selectedTarget_, targets, 4);
	ImGui::End();

	ImGui::Begin("Audio Control");
	ApplyWindowSnap("Audio Control");
	const char* bgmNames[] = { "Track 1", "Track 2" };
	ImGui::Combo("BGM", &currentBgmIndex_, bgmNames, 2);
	if (ImGui::Button("PLAY")) framework_->GetAudio()->PlayWave(soundHandles_[currentBgmIndex_], isBgmLoop_);
	ImGui::SameLine();
	if (ImGui::Button("STOP")) framework_->GetAudio()->StopWave(soundHandles_[currentBgmIndex_]);
	ImGui::Checkbox("Loop", &isBgmLoop_);
	ImGui::End();

	ImGui::Begin("Object Editor");
	ApplyWindowSnap("Object Editor");
	if (ImGui::CollapsingHeader("Sprite (2D)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::InputFloat2("Position", &spritePos_.x);
		if (ImGui::Button("Reset Pos")) spritePos_ = { 640, 360 };
		std::vector<const char*> items;
		for (const auto& n : textureNames_) items.push_back(n.c_str());
		if (ImGui::Combo("Texture", &currentSpriteTexIndex_, items.data(), (int)items.size())) {
			sprite_->SetTexture(textureHandles_[currentSpriteTexIndex_]);
		}
	}
	if (ImGui::CollapsingHeader("Particle")) {
		ImGui::InputFloat3("Emit World Pos", particleEmitPos_);
		std::vector<const char*> items;
		for (const auto& n : textureNames_) items.push_back(n.c_str());
		if (ImGui::Combo("Effect Texture", &currentParticleTexIndex_, items.data(), (int)items.size())) {
			framework_->GetParticleManager()->CreateParticleGroup("Spark", textureHandles_[currentParticleTexIndex_]);
		}
	}
	ImGui::End();

	ImGui::Begin("System Monitor");
	ApplyWindowSnap("System Monitor");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	if (ImGui::Button("Clear Logs")) debugLogs_.clear();
	ImGui::BeginChild("Logs", { 0, 100 }, true);
	for (const auto& log : debugLogs_) ImGui::TextUnformatted(log.c_str());
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::End();

	// プレイ画面上のスナップガイドとドラッグ
	if (!ImGui::GetIO().WantCaptureMouse) {
		ImVec2 m = ImGui::GetIO().MousePos;
		float sx = std::round(m.x / snapStep_) * snapStep_;
		float sy = std::round(m.y / snapStep_) * snapStep_;
		if (isSnap_) {
			ImGui::GetBackgroundDrawList()->AddCircleFilled({ sx, sy }, 6.0f, ImColor(1.0f, 1.0f, 0.0f, 0.5f));
		}
		if (ImGui::IsMouseDown(0)) {
			float tx = isSnap_ ? sx : m.x;
			float ty = isSnap_ ? sy : m.y;
			if (selectedTarget_ == 1) spritePos_ = { tx, ty };
			else if (selectedTarget_ == 3) { particleEmitPos_[0] = (tx - 640) / 50.0f; particleEmitPos_[1] = -(ty - 360) / 50.0f; }
		}
	}
#endif

	// 更新の適用
	sprite_->SetPosition(spritePos_);
	sprite_->Update();
	camera_->Update();
	object3d_->SetPosition(objectPos_);
	objectRot_.y += 0.02f;
	object3d_->SetRotation(objectRot_);
	object3d_->Update(camera_.get());
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
		if (isSnap_) { spritePos_.x = std::round(spritePos_.x / snapStep_) * snapStep_; spritePos_.y = std::round(spritePos_.y / snapStep_) * snapStep_; }
	} else if (selectedTarget_ == 2) {
		objectPos_.x += d.x * 0.1f; objectPos_.y -= d.y * 0.1f;
	} else if (selectedTarget_ == 3) {
		particleEmitPos_[0] += d.x * 0.1f; particleEmitPos_[1] -= d.y * 0.1f;
	}
}

void GamePlayScene::Draw() {
	framework_->GetObject3dCommon()->CommonDrawSettings();
	object3d_->Draw();
	framework_->GetSpriteCommon()->PreDraw();
	sprite_->Draw();
	framework_->GetParticleManager()->Draw();
}

void GamePlayScene::AddLog(const std::string& message) {
	debugLogs_.push_back(message);
	if (debugLogs_.size() > 50) debugLogs_.erase(debugLogs_.begin());
}