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
#include <dinput.h>
#include <cmath>
#include <algorithm>

/**
 * コンストラクタとデストラクタ
 */
GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

/**
 * 初期化処理
 * シーンが始まった瞬間に一度だけ呼ばれます。
 */
void GamePlayScene::Initialize() {
	// エンジンの本体を取得
	framework_ = Framework::GetInstance();
	AddLog("Scene: GamePlay Initialized.");

	// --- テクスチャの読み込み ---
	textureNames_ = {
		"monsterBall.png", "uvChecker.png", "choju8_0008.png",
		"IMG_0264.jpg", "仏顔.jpg", "魚パン.png"
	};
	textureHandles_.clear();
	for (const auto& name : textureNames_) {
		// Resources フォルダから画像を読み込んで、管理番号（ハンドル）を保存
		textureHandles_.push_back(framework_->GetSpriteCommon()->LoadTexture("Resources/" + name));
	}

	// --- 音声・3Dモデル・カメラの準備 ---
	soundHandles_[0] = framework_->GetAudio()->LoadAudio("Resources/bgm.wav");
	soundHandles_[1] = framework_->GetAudio()->LoadAudio("Resources/Player.mp3");

	// 3Dモデル（plane.obj）を読み込んで初期化
	framework_->GetModelManager()->LoadModel("plane.obj");
	Model* model = framework_->GetModelManager()->GetModel("plane.obj");
	if (model) { model->LoadTextures(framework_->GetSpriteCommon()); }

	// 3Dオブジェクトの生成
	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(framework_->GetObject3dCommon());
	object3d_->SetModel(model);

	// スプライト（2D画像）の生成
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(framework_->GetSpriteCommon(), textureHandles_[0]);

	// カメラの生成
	camera_ = std::make_unique<Camera>();

	// パーティクル（火花など）のグループを作成
	framework_->GetParticleManager()->CreateParticleGroup("Spark", textureHandles_[2]);

	// 初期座標の設定
	spritePos_ = { 640.0f, 360.0f };
	selectedTarget_ = 1; // 最初はスプライトを選択状態にする
}

/**
 * 終了処理
 * シーンが切り替わる直前に呼ばれます。
 */
void GamePlayScene::Finalize() {
	// unique_ptr を使っているため、ここでの明示的な delete は不要です
}

/**
 * 更新処理
 * 毎フレーム呼ばれ、計算や入力、UIの更新を行います。
 */
void GamePlayScene::Update() {
	// FPS（フレームレート）を記録
	frameRates_[frameRateIndex_] = ImGui::GetIO().Framerate;
	frameRateIndex_ = (frameRateIndex_ + 1) % 60;

	// キーボードによる移動処理を呼び出し
	HandleKeyboardMovement();

	// スペースキーが押されたらパーティクルを放出
	if (framework_->GetInput()->TriggerKey(DIK_SPACE)) {
		Vector3 pos = { particleEmitPos_[0], particleEmitPos_[1], particleEmitPos_[2] };
		framework_->GetParticleManager()->Emit("Spark", pos, particleEmitCount_);
		AddLog("Action: Particle Emitted.");
	}

#ifdef USE_IMGUI
	// --- ImGui ウィンドウの作成 ---

	// 1. 全般設定ウィンドウ
	ImGui::Begin("Global Settings");
	const char* targets[] = { "None", "Sprite", "Object3D", "Particle" };
	ImGui::Combo("Edit Focus", &selectedTarget_, targets, 4);
	ImGui::End();

	// 2. オーディオ操作ウィンドウ
	ImGui::Begin("Audio Control");
	const char* bgmNames[] = { "Track 1", "Track 2" };
	ImGui::Combo("BGM", &currentBgmIndex_, bgmNames, 2);
	if (ImGui::Button("PLAY")) framework_->GetAudio()->PlayWave(soundHandles_[currentBgmIndex_], isBgmLoop_);
	ImGui::SameLine();
	if (ImGui::Button("STOP")) framework_->GetAudio()->StopWave(soundHandles_[currentBgmIndex_]);
	ImGui::Checkbox("Loop", &isBgmLoop_);
	ImGui::End();

	// 3. オブジェクト編集ウィンドウ
	ImGui::Begin("Object Editor");
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

	// 4. システムモニターウィンドウ
	ImGui::Begin("System Monitor");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	if (ImGui::Button("Clear Logs")) debugLogs_.clear();
	ImGui::BeginChild("Logs", { 0, 100 }, true);
	for (const auto& log : debugLogs_) ImGui::TextUnformatted(log.c_str());
	// ログが増えたら自動で一番下までスクロール
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::End();

	// --- プレイ画面（ビューポート）上でのマウス操作 ---
	// ImGuiのボタンなどを触っていない時だけ受け付ける
	if (!ImGui::GetIO().WantCaptureMouse) {
		// ★重要：Gameクラスで計算された「補正後のマウス座標」を取得
		Vector2 m = Game::GetMousePosInViewport();

		if (ImGui::IsMouseDown(0)) {
			// スプライトが選択されているなら、マウス位置へ移動（スナップなし）
			if (selectedTarget_ == 1) {
				spritePos_ = m;
			}
			// パーティクルが選択されているなら、放出位置を変更
			else if (selectedTarget_ == 3) {
				particleEmitPos_[0] = (m.x - 640.0f) / 50.0f;
				particleEmitPos_[1] = -(m.y - 360.0f) / 50.0f;
			}
		}
	}
#endif

	// 各オブジェクトの更新を適用（ここで計算された座標が行列に変換されます）
	sprite_->SetPosition(spritePos_);
	sprite_->Update();
	camera_->Update();

	object3d_->SetPosition(objectPos_);
	objectRot_.y += 0.02f; // 自動で少しずつ回転させる
	object3d_->SetRotation(objectRot_);
	object3d_->Update(camera_.get());

	framework_->GetParticleManager()->Update(camera_.get());
}

/**
 * キーボード移動
 * 矢印キーでオブジェクトを動かします。
 * ★修正：クラス名の記述を Game:: から GamePlayScene:: へ修正しました
 */
void GamePlayScene::HandleKeyboardMovement() {
	// 何も選択されていないか、ImGuiがキー入力を奪っている時は何もしない
	if (selectedTarget_ == 0 || ImGui::GetIO().WantCaptureKeyboard) return;

	// 左シフトキーを押している間は移動速度を10倍にする
	float val = framework_->GetInput()->PushKey(DIK_LSHIFT) ? 10.0f : 1.0f;
	Vector2 d = { 0, 0 };
	if (framework_->GetInput()->PushKey(DIK_UP))    d.y -= val;
	if (framework_->GetInput()->PushKey(DIK_DOWN))  d.y += val;
	if (framework_->GetInput()->PushKey(DIK_LEFT))  d.x -= val;
	if (framework_->GetInput()->PushKey(DIK_RIGHT)) d.x += val;

	// 選択中のターゲットに合わせて座標を加算
	if (selectedTarget_ == 1) {
		spritePos_.x += d.x;
		spritePos_.y += d.y;
	} else if (selectedTarget_ == 2) {
		objectPos_.x += d.x * 0.1f;
		objectPos_.y -= d.y * 0.1f;
	} else if (selectedTarget_ == 3) {
		particleEmitPos_[0] += d.x * 0.1f;
		particleEmitPos_[1] -= d.y * 0.1f;
	}
}

/**
 * 描画処理
 * モデル、スプライト、パーティクルの順に描画命令を出します。
 */
void GamePlayScene::Draw() {
	// 3Dモデルの共通設定と描画
	framework_->GetObject3dCommon()->CommonDrawSettings();
	object3d_->Draw();

	// スプライト（2D）の共通設定と描画
	framework_->GetSpriteCommon()->PreDraw();
	sprite_->Draw();

	// パーティクルの描画
	framework_->GetParticleManager()->Draw();
}

/**
 * ログ追加ヘルパー
 */
void GamePlayScene::AddLog(const std::string& message) {
	debugLogs_.push_back(message);
	// ログが溜まりすぎないように 50件を超えたら古い順に消す
	if (debugLogs_.size() > 50) debugLogs_.erase(debugLogs_.begin());
}