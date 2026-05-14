#include "scene/GamePlayScene.h"
#include "base/Framework.h"
#include "2d/Sprite.h"
#include "3d/Object3d.h"
#include "3d/Model.h"
#include "3d/Camera.h"
#include "audio/Audio.h"
#include "io/Input.h"
#include "base/ImGuiManager.h"
#include "3d/ModelManager.h"
#include "effect/ParticleManager.h"
#include "2d/SpriteCommon.h"
#include "3d/Object3dCommon.h"
#include "Game.h"
#include "3d/PrimitiveGenerator.h"
#include <dinput.h>
#include "3d/Skybox.h"
#include "base/Logger.h" // 追加：外部ロガーツールのインクルード
#include <random>

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

/**
 * Initialize: シーン開始時に一度だけ呼ばれるセットアップ関数
 */
void GamePlayScene::Initialize() {
	framework_ = Framework::GetInstance();

	// ログ記録：UIと外部出力の両方に行われます
	AddLog("Scene: GamePlay Initialized.");

	// ---------------------------------------------------------
	// 1. テクスチャ・リソースの読み込み
	// ---------------------------------------------------------
	textureHandles_.clear();
	std::vector<std::string> textureNames = { "monsterBall.png", "uvChecker.png", "choju8_0008.png" };
	for (const auto& name : textureNames) {
		textureHandles_.push_back(framework_->GetSpriteCommon()->LoadTexture("Resources/" + name));
	}
	uint32_t circle2Handle = framework_->GetSpriteCommon()->LoadTexture("Resources/circle2.png");
	uint32_t ringTexHandle = framework_->GetSpriteCommon()->LoadTexture("Resources/gradationLine.png");

	// ---------------------------------------------------------
	// 2. システム・環境の初期化
	// ---------------------------------------------------------
	camera_ = std::make_unique<Camera>();
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(framework_->GetDxCommon(), framework_->GetSrvManager(), "Resources/rostock_laage_airport_4k.dds");

	// ---------------------------------------------------------
	// 3. モデルデータのロード
	// ---------------------------------------------------------
	std::string terrainPath = "terrain/terrain.obj";
	framework_->GetModelManager()->LoadModel(terrainPath);
	Model* tModel = framework_->GetModelManager()->GetModel(terrainPath);
	if (tModel) { tModel->LoadTextures(framework_->GetSpriteCommon()); }

	framework_->GetModelManager()->LoadModel("plane.obj");
	Model* planeModel = framework_->GetModelManager()->GetModel("plane.obj");

	// アニメーションモデルのロード
	std::string animModelPath = "AnimatedCube/AnimatedCube.gltf";
	framework_->GetModelManager()->LoadModel(animModelPath);
	Model* animModel = framework_->GetModelManager()->GetModel(animModelPath);
	if (animModel) {
		animModel->LoadTextures(framework_->GetSpriteCommon());
	}

	// ---------------------------------------------------------
	// 4. 各種オブジェクトの実体生成
	// ---------------------------------------------------------
	terrainObj_ = std::make_unique<Object3d>();
	terrainObj_->Initialize(framework_->GetObject3dCommon());
	terrainObj_->SetModel(tModel);
	terrainObj_->SetPosition({ 0.0f, -2.0f, 0.0f });
	terrainObj_->SetScale({ 2.0f, 2.0f, 2.0f });

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(framework_->GetObject3dCommon());
	object3d_->SetModel(planeModel);

	if (animModel) {
		animObj_ = std::make_unique<Object3d>();
		animObj_->Initialize(framework_->GetObject3dCommon());
		animObj_->SetModel(animModel);
		animObj_->SetPosition({ 5.0f, 0.0f, 0.0f });
		animObj_->SetScale({ 1.0f, 1.0f, 1.0f });
		// アニメーションを読み込んでセット
		Animation anim = animModel->LoadAnimation("Resources/AnimatedCube", "AnimatedCube.gltf");
		animObj_->SetAnimation(anim);
	}

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(framework_->GetSpriteCommon(), textureHandles_[0]);

	CreateSphere(sphereRadius_);

	ringModel_ = PrimitiveGenerator::CreateRing(framework_->GetModelManager(), 0.8f, 1.0f, 32);
	// 円柱：メンバ変数のパラメータで生成（ImGuiから変更可能）
	cylinderModel_ = PrimitiveGenerator::CreateCylinder(
		framework_->GetModelManager(),
		cylTopRadius_ ,cylBottomRadius_, cylHeight_,
		static_cast<uint32_t>(cylSegments_),
		static_cast<uint32_t>(cylVertDivisions_));


	// ---------------------------------------------------------
	// 5. パーティクルの設定
	// ---------------------------------------------------------
	framework_->GetParticleManager()->CreateParticleGroup("Spark", circle2Handle);
	framework_->GetParticleManager()->CreateParticleGroup("RingEffect", ringTexHandle, ringModel_.get());
	framework_->GetParticleManager()->CreateParticleGroup("CylinderEffect", ringTexHandle, cylinderModel_.get());


}

void GamePlayScene::EmitSpark(const Vector3& position) {
	std::random_device seed_gen;
	std::mt19937 randomEngine(seed_gen());
	std::uniform_real_distribution<float> distRotate(-3.141592f, 3.141592f);
	std::uniform_real_distribution<float> distScale(0.8f, 1.5f);

	for (int i = 0; i < 8; ++i) {
		Particle& p = framework_->GetParticleManager()->AddParticle("Spark", position);
		p.transform.scale = { 0.05f, distScale(randomEngine), 1.0f };
		p.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine) };
		p.color = { 1.0f, 1.0f, 0.6f, 1.0f };
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.lifeTime = 0.2f;
	}
}

void GamePlayScene::EmitRingEffect(const Vector3& position) {
	Particle& p = framework_->GetParticleManager()->AddParticle("RingEffect", position);

	//地面を水平にする
	p.transform.rotate = {std::numbers::pi_v<float> /2.0f, 0.0f, 0.0f};
	p.velocity = { 0.0f,0.0f,0.0f };
	p.color = { 1.0f, 1.0f, 1.0f, 0.8f };
	p.lifeTime = 3.0f;

	// Ringが徐々に広がる設定
	p.startSize = 0.5f;
	p.endSize = 3.0f;

	// UVスクロールの設定（テクスチャが回転するように見える）
	// 資料の「U方向にScaleすれば解像度が…」の対応
	p.uvScale = { 2.0f, 1.0f };
	// V方向にテクスチャーをスクロールさせる
	p.uvVelocity = { 0.0f, -1.5f };
}
// 機能：円柱エフェクトの放出関数
void GamePlayScene::EmitCylinderEffect(const Vector3& position) {
	Particle& p = framework_->GetParticleManager()->AddParticle("CylinderEffect", position);
	p.transform.rotate = { 0.0f, 0.0f, 0.0f };
	p.color = { 0.4f, 0.7f, 1.0f, 1.0f }; // 少し青みを強めに
	p.lifeTime = 1.5f;
	p.velocity = { 0.0f, 0.01f, 0.0f };
	p.startSize = 0.5f;
	p.endSize = 3.0f;

	// 資料：V Flip (v = 1-v) と 横方向スクロール
	p.uvScale = { 2.0f, -1.0f }; // U方向に2倍(解像度アップ)、V方向に反転
	p.uvOffset = { 0.0f, 1.0f };
	p.uvVelocity = { 1.5f, 0.0f }; // U方向にスクロール
}

void GamePlayScene::Update() {
	HandleKeyboardMovement();

	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);
	camera_->Update();

	sprite_->SetPosition(spritePos_);
	sprite_->Update();

	skybox_->Update(camera_.get());

	Vector3 normSpotDir = MatrixMath::Normalize(spotLightDir_);

	auto UpdateObjectLights = [&](Object3d* obj, float envCoef) {
		if (!obj) return;
		obj->SetCullMode(cullMode_);
		obj->SetLightDirection(lightDirection_);
		obj->SetLightColor({ lightColor_.x, lightColor_.y, lightColor_.z, 1.0f });
		obj->SetLightIntensity(lightIntensity_);

		obj->SetSpotLightColor({ spotLightColor_.x, spotLightColor_.y, spotLightColor_.z, 1.0f });
		obj->SetSpotLightPosition(spotLightPos_);
		obj->SetSpotLightDirection(normSpotDir);
		obj->SetSpotLightDistance(spotLightDistance_);
		obj->SetSpotLightIntensity(spotLightIntensity_);
		obj->SetSpotLightDecay(spotLightDecay_);
		obj->SetSpotLightAngle(spotLightAngle_);
		obj->SetSpotLightFalloff(spotLightFalloff_);

		obj->SetEnvironmentMap(skybox_->GetSrvIndex());
		obj->SetEnvironmentCoefficient(envCoef);

		obj->Update(camera_.get());
		};

	UpdateObjectLights(terrainObj_.get(), 0.0f);
	UpdateObjectLights(object3d_.get(), 0.0f);
	UpdateObjectLights(animObj_.get(), 0.5f);

	if (sphereObj_) {
		sphereObj_->SetPosition(spherePos_);
		sphereObj_->SetRotation(objectRot_);
		UpdateObjectLights(sphereObj_.get(), 0.5f);
	}

	// スペースキー入力時の分岐（activeParticleType_ は Game.cpp の ImGui から書き換わる）
	if (framework_->GetInput()->TriggerKey(DIK_SPACE)) {
		switch (activeParticleType_) {
		case 0: EmitSpark(spherePos_); break;
		case 1: EmitRingEffect(spherePos_); break;
		case 2: EmitCylinderEffect(spherePos_); break; // ★Cylinder単体
		case 3: // Combined
			EmitRingEffect(spherePos_);
			EmitCylinderEffect(spherePos_);
			break;
		case 4: // Explosion
			framework_->GetParticleManager()->Emit("Spark", spherePos_, 20);
			break;
		}
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

/**
 * RebuildCylinder: Cylinderメッシュの再生成
 * ImGuiでパラメータを変更したときに呼ばれる
 * 新しいメッシュを生成し、ParticleGroupのモデルポインタも更新する
 */
void GamePlayScene::RebuildCylinder() {
	cylinderModel_ = PrimitiveGenerator::CreateCylinder(
		framework_->GetModelManager(),
		cylTopRadius_, cylBottomRadius_, cylHeight_,
		static_cast<uint32_t>(cylSegments_),
		static_cast<uint32_t>(cylVertDivisions_));
	// ParticleGroupのモデルポインタも更新しないと古いメッシュを参照し続ける
	framework_->GetParticleManager()->CreateParticleGroup(
		"CylinderEffect",
		framework_->GetSpriteCommon()->LoadTexture("Resources/gradationLine.png"),
		cylinderModel_.get());
}

void GamePlayScene::Draw() {
	auto objCommon = framework_->GetObject3dCommon();
	auto spriteCommon = framework_->GetSpriteCommon();

	objCommon->CommonDrawSettings();

	if (modelPriority_ == 0) {
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		if (skybox_) skybox_->Draw();

		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();
	}
	else {
		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();

		objCommon->CommonDrawSettings();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		if (skybox_) skybox_->Draw();
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

/**
 * AddLog: デバッグログを記録する関数
 */
void GamePlayScene::AddLog(const std::string& message) {
	// 1. 外部ロガーツールを使用して Visual Studio の出力ウィンドウへ出す
	Logger::Log(message);

	// 2. ゲーム内 UI 用のリストに追加
	debugLogs_.push_back(message);

	// 3. 履歴の制限
	if (debugLogs_.size() > 50) {
		debugLogs_.erase(debugLogs_.begin());
	}
}

void GamePlayScene::Finalize() {}