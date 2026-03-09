#include "Game.h"

/**
 * 初期化：ゲーム開始時に一度だけ呼ばれる
 */
void Game::Initialize() {
	// 1. システム基盤の初期化
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	dxCommon_ = std::make_unique<DirectXCommon>();
	dxCommon_->Initialize(winApp_.get());

	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Initialize(dxCommon_.get());

	input_ = std::make_unique<Input>();
	input_->Initialize(winApp_.get());

	audio_ = std::make_unique<Audio>();
	audio_->Initialize();

	// 2. 描画マネージャの初期化
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get(), srvManager_.get());

	object3dCommon_ = std::make_unique<Object3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get(), srvManager_.get());

	modelManager_ = std::make_unique<ModelManager>();
	modelManager_->Initialize(dxCommon_.get(), srvManager_.get());

	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Initialize(dxCommon_.get(), srvManager_.get());

	// ImGuiの初期化
	ImGuiManager::GetInstance()->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());

	// 3. リソースのロード
	// BGM (WAVとMP3)
	soundHandles_[0] = audio_->LoadAudio("Resources/bgm.wav");
	soundHandles_[1] = audio_->LoadAudio("Resources/Player.mp3");

	// テクスチャ
	texMonsterHandle_ = spriteCommon_->LoadTexture("Resources/monsterBall.png");
	texWhiteHandle_ = spriteCommon_->LoadTexture("Resources/white.png");

	// 4. ゲームオブジェクトの生成
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(spriteCommon_.get(), texMonsterHandle_);

	modelManager_->LoadModel("plane.obj");
	Model* model = modelManager_->GetModel("plane.obj");
	if (model) { model->LoadTextures(spriteCommon_.get()); }

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(object3dCommon_.get());
	object3d_->SetModel(model);

	camera_ = std::make_unique<Camera>();

	// 最初の曲を再生開始
	audio_->PlayWave(soundHandles_[currentBgmIndex_], isBgmLoop_);
}

/**
 * 更新：毎フレーム呼ばれる
 */
void Game::Update() {
	// メッセージ処理（ウィンドウが閉じられたら終了リクエストを出す）
	if (winApp_->ProcessMessage()) {
		isEndRequest_ = true;
		return;
	}

	// 入力情報の更新
	input_->Update();

	// スペースキーで現在の曲をリスタート
	if (input_->TriggerKey(DIK_SPACE)) {
		audio_->PlayWave(soundHandles_[currentBgmIndex_], isBgmLoop_);
	}

	// ImGuiの受付開始
	ImGuiManager::GetInstance()->Begin();

#ifdef USE_IMGUI
	// 背景透過
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	// A. 音楽プレイヤー設定
	ImGui::Begin("Audio Selector");
	const char* bgmNames[] = { "Original (WAV)", "Player (MP3)" };
	ImGui::Combo("Select Track", &currentBgmIndex_, bgmNames, IM_ARRAYSIZE(bgmNames));
	ImGui::Checkbox("Loop", &isBgmLoop_);
	if (ImGui::Button("Play BGM")) { audio_->PlayWave(soundHandles_[currentBgmIndex_], isBgmLoop_); }
	ImGui::SameLine();
	if (ImGui::Button("Stop BGM")) { audio_->StopWave(soundHandles_[currentBgmIndex_]); }
	ImGui::End();

	// B. 描画順の設定
	ImGui::Begin("Priority Settings");
	ImGui::Text("[ Layer: Model vs Sprite ]");
	ImGui::RadioButton("Sprite Front", &modelPriority_, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Model Front", &modelPriority_, 1);
	ImGui::Separator();
	ImGui::Text("[ Layer: Sprite vs Particle ]");
	if (ImGui::RadioButton("Sprite on Top", spriteOnTopVsParticle_ == true)) { spriteOnTopVsParticle_ = true; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Particle on Top", spriteOnTopVsParticle_ == false)) { spriteOnTopVsParticle_ = false; }
	ImGui::End();

	// C. 操作エディタ
	ImGui::Begin("Game Editor");
	if (ImGui::CollapsingHeader("Sprite Pos", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat2("Pos", &spritePos_.x, 0.0f, 1280.0f, "%.1f");
	}
	if (ImGui::CollapsingHeader("Particles")) {
		if (ImGui::Button("Emit (Spark)")) {
			particleManager_->Emit("Spark", { 0, 0, 0 }, 10);
		}
	}
	ImGui::End();

	ImGui::ShowDemoWindow();
#endif

	// オブジェクトの更新
	sprite_->SetPosition(spritePos_);
	sprite_->Update();

	camera_->Update();

	object3d_->SetRotation({ 0.0f, object3d_->GetRotation().y + 0.02f, 0.0f });
	object3d_->Update(camera_.get());

	// パーティクルグループがない場合は作る（初回のみ）
	particleManager_->CreateParticleGroup("Spark", texWhiteHandle_);
	particleManager_->Update(camera_.get());

	// ImGuiの受付終了
	ImGuiManager::GetInstance()->End();
}

/**
 * 描画：毎フレーム呼ばれる
 */
void Game::Draw() {
	// 描画前処理
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	// フラグに応じた描画順序の入れ替えロジック
	if (modelPriority_ == 1) {
		// --- モデルが一番手前 ---
		spriteCommon_->PreDraw();
		sprite_->Draw();
		object3dCommon_->CommonDrawSettings();
		object3d_->Draw();
	} else {
		// --- スプライトがモデルより手前 ---
		object3dCommon_->CommonDrawSettings();
		object3d_->Draw();
		spriteCommon_->PreDraw();
		sprite_->Draw();
	}

	// パーティクルの描画（最前面の設定の場合）
	if (!spriteOnTopVsParticle_) {
		particleManager_->Draw();
	}

	// ImGuiの描画（常に最前面）
	ImGuiManager::GetInstance()->Draw();

	// 描画後処理（画面を切り替えて表示）
	dxCommon_->PostDraw();
}

/**
 * 終了処理：ゲームが終わるとき一度だけ呼ばれる
 */
void Game::Finalize() {
	ImGuiManager::GetInstance()->Finalize();
	audio_->Finalize();
	winApp_->Finalize();
}