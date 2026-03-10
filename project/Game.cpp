#include "Game.h"

// --- ここで必要なクラスの中身（ヘッダー）をすべて読み込む ---
// これを忘れると「不完全な型」というエラーになります。
// ここで読み込むことで、コンパイラは各クラスの「サイズ」や「壊し方」を理解できます。
#include "Sprite.h"
#include "Object3d.h"
#include "Model.h"
#include "Camera.h"
#include "Audio.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"

// キーボードのキーの名前（DIK_SPACE など）を使えるようにします。
#include <dinput.h>

/**
 * ★ここがエラー解消の核心です。
 * 全てのヘッダーを読み込んだ「後」で default 定義をすることで、
 * unique_ptr が Camera や Model を安全に扱えるようになります。
 */
Game::Game() = default;
Game::~Game() = default;

/**
 * 初期化：ゲーム開始時に一度だけ呼ばれる準備処理
 */
void Game::Initialize() {
	// 1. まず親クラス(Framework)の初期化を呼び、DirectX基盤等を整えます。
	Framework::Initialize();

	// 2. 音声ファイルをメモリに読み込む（Media Foundation対応済み）
	bgmHandles_[0] = audio_->LoadAudio("Resources/bgm.wav");
	bgmHandles_[1] = audio_->LoadAudio("Resources/Player.mp3");

	// 3. テクスチャ（画像）の読み込み
	uint32_t monsterTex = spriteCommon_->LoadTexture("Resources/monsterBall.png");
	uint32_t whiteTex = spriteCommon_->LoadTexture("Resources/white.png");

	// 4. 各種オブジェクトの作成とセットアップ
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon_.get(), monsterTex);

	// 3Dモデル（Resourcesフォルダ内のplane.obj）
	modelManager_->LoadModel("plane.obj");
	Model* model = modelManager_->GetModel("plane.obj");
	if (model) { model->LoadTextures(spriteCommon_.get()); }

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCommon_.get());
	object3d_->SetModel(model);

	// パーティクルの受け皿（グループ）を作成
	particleManager_->CreateParticleGroup("Spark", whiteTex);

	// カメラの作成
	camera_ = std::make_unique<Camera>();

	// 最初の一曲を自動再生
	audio_->PlayWave(bgmHandles_[currentBgmIndex_], isBgmLoop_);
}

/**
 * 更新：毎フレームの計算や入力を受け付ける処理
 */
void Game::Update() {
	// 1. フレームワークの基本処理（メッセージ処理、入力更新）
	Framework::Update();

	// 2. スペースキーが押されたら現在選択中のBGMを再生（デバッグ用）
	if (input_->TriggerKey(DIK_SPACE)) {
		audio_->PlayWave(bgmHandles_[currentBgmIndex_], isBgmLoop_);
	}

	// --- 3. ImGuiによるデバッグ操作画面の定義開始 ---
	ImGuiManager::GetInstance()->Begin();

#ifdef USE_IMGUI
	// 背景を透過させてゲーム画面を見えるようにする
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	// A. 音楽プレイヤー設定
	ImGui::Begin("Audio Selector");
	const char* names[] = { "Original (WAV)", "Player (MP3)" };
	if (ImGui::Combo("Select Track", &currentBgmIndex_, names, 2)) {
		// 変更があった際、ここでも自動再生させるなどの処理が書けます
	}
	ImGui::Checkbox("Loop Playback", &isBgmLoop_);
	if (ImGui::Button("Play Selected", ImVec2(120, 30))) {
		audio_->PlayWave(bgmHandles_[currentBgmIndex_], isBgmLoop_);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop BGM", ImVec2(120, 30))) {
		audio_->StopWave(bgmHandles_[currentBgmIndex_]);
	}
	ImGui::End();

	// B. 描画の優先順位（レイヤー）設定
	ImGui::Begin("Layer Controls");
	ImGui::Text("[ Sprite vs OBJ Model ]");
	ImGui::RadioButton("Sprite Front", &modelPriority_, 0); ImGui::SameLine();
	ImGui::RadioButton("Model Front", &modelPriority_, 1);
	ImGui::Separator();
	ImGui::Checkbox("Sprite Over Particles", &spriteOnTopVsParticle_);
	ImGui::End();

	// C. 各種パラメータ操作
	ImGui::Begin("Object Editor");
	ImGui::SliderFloat2("Sprite Pos", &spritePos_.x, 0.0f, 1280.0f);
	if (ImGui::Button("Emit 10 Particles")) {
		particleManager_->Emit("Spark", { 0,0,0 }, 10);
	}
	ImGui::End();
#endif

	// 4. オブジェクトの動きを反映
	sprite_->SetPosition(spritePos_);
	sprite_->Update();

	camera_->Update();

	// 板ポリ（3D）をくるくる回す
	object3d_->SetRotation({ 0, object3d_->GetRotation().y + 0.02f, 0 });
	object3d_->Update(camera_.get());

	particleManager_->Update(camera_.get());

	// ImGuiの定義終了
	ImGuiManager::GetInstance()->End();
}

/**
 * 描画：最終的な絵を画面に出力する処理
 */
void Game::Draw() {
	// 描画の準備
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	// 【描画のルール】プログラムで「後から」命令したものが手前に重なります。

	if (modelPriority_ == 1) {
		// --- パターン：モデルを一番手前にする場合 ---
		spriteCommon_->PreDraw();
		sprite_->Draw();
		object3dCommon_->CommonDrawSettings();
		object3d_->Draw();
	} else {
		// --- パターン：スプライトをモデルより手前にする場合 ---
		object3dCommon_->CommonDrawSettings();
		object3d_->Draw();
		spriteCommon_->PreDraw();
		sprite_->Draw();
	}

	// パーティクルの描画（最前面の設定の場合のみ描画）
	if (!spriteOnTopVsParticle_) {
		particleManager_->Draw();
	}

	// UI描画（常に最前面）
	ImGuiManager::GetInstance()->Draw();

	// 描画終了
	dxCommon_->PostDraw();
}

/**
 * 終了処理：ゲームが終わる際の後片付け
 */
void Game::Finalize() {
	// 最後に親クラス(Framework)の共通終了処理を呼んでシステムを閉じます。
	Framework::Finalize();
}