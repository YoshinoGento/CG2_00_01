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
#include "3d/LineDrawer.h"
#include <dinput.h>
#include "3d/SkyboxManager.h"
#include "base/Logger.h" // 追加：外部ロガーツールのインクルード
#include "FieldManager.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr float kVirtualScreenWidth = 1280.0f;
constexpr float kVirtualScreenHeight = 720.0f;

GPUParticleEmitSettings MakeAgricultureEmitSettings(
	const Vector3& position,
	float particleSize,
	uint32_t count,
	GamePlayScene::AgricultureParticleType type) {
	GPUParticleEmitSettings settings{};
	settings.translate = position;
	settings.radius = 0.8f;
	settings.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	settings.scale = { particleSize, particleSize, particleSize };
	settings.lifeTime = 1.0f;
	settings.baseVelocity = { 0.0f, 0.3f, 0.0f };
	settings.speed = 0.5f;
	settings.count = count;
	settings.emit = 1;
	settings.preset = static_cast<uint32_t>(type);

	switch (type) {
	case GamePlayScene::AgricultureParticleType::DirtDust:
		settings.radius = 0.65f;
		settings.color = { 0.45f, 0.28f, 0.12f, 1.0f };
		settings.scale = { particleSize * 0.65f, particleSize * 0.65f, particleSize * 0.65f };
		settings.lifeTime = 0.75f;
		settings.baseVelocity = { 0.0f, 0.35f, 0.0f };
		settings.speed = 0.25f;
		break;
	case GamePlayScene::AgricultureParticleType::WaterSplash:
		settings.radius = 0.45f;
		settings.color = { 0.45f, 0.85f, 1.0f, 1.0f };
		settings.scale = { particleSize * 0.55f, particleSize * 0.55f, particleSize * 0.55f };
		settings.lifeTime = 0.65f;
		settings.baseVelocity = { 0.0f, 0.15f, 0.0f };
		settings.speed = 1.25f;
		break;
	case GamePlayScene::AgricultureParticleType::HarvestSparkle:
		settings.radius = 0.9f;
		settings.color = { 1.0f, 0.9f, 0.25f, 1.0f };
		settings.scale = { particleSize * 0.45f, particleSize * 0.45f, particleSize * 0.45f };
		settings.lifeTime = 1.25f;
		settings.baseVelocity = { 0.0f, 0.85f, 0.0f };
		settings.speed = 0.35f;
		break;
	case GamePlayScene::AgricultureParticleType::PollenSpore:
		settings.radius = 1.2f;
		settings.color = { 0.6f, 0.95f, 0.25f, 1.0f };
		settings.scale = { particleSize * 0.4f, particleSize * 0.4f, particleSize * 0.4f };
		settings.lifeTime = 2.0f;
		settings.baseVelocity = { 0.15f, 0.25f, 0.0f };
		settings.speed = 0.15f;
		break;
	case GamePlayScene::AgricultureParticleType::BugSwarm:
		settings.radius = 1.0f;
		settings.color = { 0.08f, 0.25f, 0.06f, 1.0f };
		settings.scale = { particleSize * 0.35f, particleSize * 0.35f, particleSize * 0.35f };
		settings.lifeTime = 1.8f;
		settings.baseVelocity = { 0.0f, 0.1f, 0.0f };
		settings.speed = 0.45f;
		break;
	}

	return settings;
}
}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

bool GamePlayScene::ConsumeFieldHarvestEvent(Vector3& outPosition, int32_t& outPrice, bool& outRare) {
	if (!fieldManager_) {
		return false;
	}
	return fieldManager_->ConsumeHarvestEvent(outPosition, outPrice, outRare);
}

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
	ringTexHandle_ = framework_->GetSpriteCommon()->LoadTexture("Resources/gradationLine.png");

	// ---------------------------------------------------------
	// 2. システム・環境の初期化
	// ---------------------------------------------------------
	camera_ = std::make_unique<Camera>();
	skyboxManager_ = std::make_unique<SkyboxManager>();
	skyboxManager_->Initialize(framework_->GetDxCommon(), framework_->GetSrvManager());
	const std::string defaultSkyboxPath = "Resources/rostock_laage_airport_4k.dds";
	skyboxManager_->LoadSkybox("Sunny", defaultSkyboxPath);
	skyboxManager_->LoadSkybox("Evening", defaultSkyboxPath);
	skyboxManager_->LoadSkybox("Night", defaultSkyboxPath);
	skyboxManager_->LoadSkybox("Storm", defaultSkyboxPath);
	skyboxManager_->SetMode(SkyboxManager::SkyboxMode::SolidColor);

	LineDrawer::GetInstance()->Initialize(framework_->GetDxCommon());

	skeletonDebugger_ = std::make_unique<SkeletonDebugger>();
	skeletonDebugger_->Initialize(framework_->GetObject3dCommon(), framework_->GetModelManager());
	skeletonDebugger_->SetEnvironmentMap(skyboxManager_->GetCurrentSrvIndex());

	fieldManager_ = std::make_unique<FieldManager>();
	fieldManager_->Initialize(framework_);

	// ---------------------------------------------------------
	// 3. モデルデータのロード
	// ---------------------------------------------------------
	std::string terrainPath = "terrain/terrain.obj";
	framework_->GetModelManager()->LoadModel(terrainPath);
	Model* tModel = framework_->GetModelManager()->GetModel(terrainPath);
	if (tModel) { tModel->LoadTextures(framework_->GetSpriteCommon()); }

	framework_->GetModelManager()->LoadModel("plane.obj");
	Model* planeModel = framework_->GetModelManager()->GetModel("plane.obj");

	// アニメーションモデルのプリロード（配布データ3種と仮モデル1種）
	std::vector<std::pair<std::string, std::string>> animModels = {
		{ "AnimatedCube", "AnimatedCube.gltf" },
		{ "simpleSkin", "simpleSkin.gltf" },
		{ "human", "walk.gltf" },
		{ "human", "sneakWalk.gltf" }
	};

	for (const auto& pair : animModels) {
		std::string path = pair.first + "/" + pair.second;
		framework_->GetModelManager()->LoadModel(path);
		Model* m = framework_->GetModelManager()->GetModel(path);
		if (m) {
			// テクスチャをSpriteCommonにロードする
			m->LoadTextures(framework_->GetSpriteCommon());
		}
	}

	// ---------------------------------------------------------
	// 4. 各種オブジェクトの実体生成
	// ---------------------------------------------------------
	terrainObj_ = std::make_unique<Object3d>();
	terrainObj_->Initialize(framework_->GetObject3dCommon());
	terrainObj_->SetModel(tModel);
	terrainObj_->SetPosition({ 0.0f, -2.0f, 0.0f });
	terrainObj_->SetScale({ 2.0f, 2.0f, 2.0f });
	terrainObj_->SetEnvironmentMap(skyboxManager_->GetCurrentSrvIndex());

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(framework_->GetObject3dCommon());
	object3d_->SetModel(planeModel);
	object3d_->SetEnvironmentMap(skyboxManager_->GetCurrentSrvIndex());

	// デフォルトで simpleSkin.gltf (配布データ) をアニメーション表示オブジェクトとしてロード
	currentAnimModelIdx_ = 1;
	ChangeAnimationModel(currentAnimModelIdx_);

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
	framework_->GetParticleManager()->CreateParticleGroup("RingEffect", ringTexHandle_, ringModel_.get());
	framework_->GetParticleManager()->CreateParticleGroup("CylinderEffect", ringTexHandle_, cylinderModel_.get());


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
	UpdateSceneDeltaTime();
	HandleCameraInput(sceneDeltaTime_);
	ClampCameraPitch();
	HandleKeyboardMovement();

	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);
	camera_->Update();
	HandleFieldMouseSelection();

	sprite_->SetPosition(spritePos_);
	sprite_->Update();

	if (skyboxManager_) {
		if (framework_->GetInput() && framework_->GetInput()->TriggerKey(DIK_F6) && !ImGui::GetIO().WantCaptureKeyboard) {
			skyboxManager_->CycleMode();
		}
		framework_->GetDxCommon()->SetSceneClearColor(skyboxManager_->GetClearColor());
		skyboxManager_->Update(camera_.get());
	}

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

		obj->SetEnvironmentMap(skyboxManager_ ? skyboxManager_->GetCurrentSrvIndex() : 0);
		obj->SetEnvironmentCoefficient(envCoef);

		obj->Update(camera_.get());
		};

	UpdateObjectLights(terrainObj_.get(), 0.0f);
	UpdateObjectLights(object3d_.get(), 0.0f);
	UpdateObjectLights(animObj_.get(), 0.5f);
	if (fieldManager_) {
		fieldManager_->Update(sceneDeltaTime_, camera_.get());
	}

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

	SyncGPUParticleDebugModeChange();
	HandleGPUParticleDebugModeInput();

	if (gpuParticleDebugMode_ == GPUParticleDebugMode::Off && framework_->GetInput()->TriggerKey(DIK_G)) {
		framework_->GetParticleManager()->RequestGPUParticleEmit(spherePos_, 256);
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
		ringTexHandle_,
		cylinderModel_.get());
}

void GamePlayScene::Draw() {
	auto objCommon = framework_->GetObject3dCommon();
	auto spriteCommon = framework_->GetSpriteCommon();

	objCommon->CommonDrawSettings();

	// === 地面のグリッド（格子線）の描画 ===
	const float gridScale = 20.0f; // グリッドの広さ
	const int divCount = 10;       // 分割数
	const Vector4 gridColor = { 0.5f, 0.5f, 0.5f, 1.0f }; // グレー

	for (int i = -divCount; i <= divCount; ++i) {
		float f = (float)i / (float)divCount * gridScale;
		// X軸に平行な線
		LineDrawer::GetInstance()->DrawLine({ -gridScale, -2.0f, f }, { gridScale, -2.0f, f }, gridColor);
		// Z軸に平行な線
		LineDrawer::GetInstance()->DrawLine({ f, -2.0f, -gridScale }, { f, -2.0f, gridScale }, gridColor);
	}

	if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction &&
		interactionBrushOperation_ != InteractionBrushOperation::None) {
		const Vector4 brushColor =
			interactionBrushOperation_ == InteractionBrushOperation::Pull
			? Vector4{ 0.1f, 0.75f, 1.0f, 1.0f }
			: Vector4{ 1.0f, 0.35f, 0.05f, 1.0f };
		LineDrawer::GetInstance()->DrawWireSphere(
			interactionBrushPosition_,
			(std::max)(interactionBrushRadius_, 0.01f),
			brushColor,
			24);
	}

	if (modelPriority_ == 0) {
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (fieldManager_) fieldManager_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		if (skyboxManager_) skyboxManager_->Draw();

		if (showSkeleton_ && animObj_ && animObj_->GetSkeleton()) {
			// 【修正】モデルルート行列の二重掛けを防ぐため、GetWorldMatrix() ではなくルートを含まない GetObjectWorldMatrix() を渡します
			skeletonDebugger_->Draw(*animObj_->GetSkeleton(), animObj_->GetObjectWorldMatrix(), LineDrawer::GetInstance(), camera_.get());
		}
		// その直後に呼ばれる
		LineDrawer::GetInstance()->Draw(camera_->GetViewProjectionMatrix());
	}
	else {
		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();

		objCommon->CommonDrawSettings();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (fieldManager_) fieldManager_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		if (skyboxManager_) skyboxManager_->Draw();

		if (showSkeleton_ && animObj_ && animObj_->GetSkeleton()) {
			// 【修正】モデルルート行列の二重掛けを防ぐため、GetWorldMatrix() ではなくルートを含まない GetObjectWorldMatrix() を渡します
			skeletonDebugger_->Draw(*animObj_->GetSkeleton(), animObj_->GetObjectWorldMatrix(), LineDrawer::GetInstance(), camera_.get());
		}
		LineDrawer::GetInstance()->Draw(camera_->GetViewProjectionMatrix());
	}

	if (showParticles_) {
		ParticleManager* particleManager = framework_->GetParticleManager();
		if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
			particleManager->Draw(false);

			GPUParticleInteractionSettings settings{};
			const uint32_t gridCount = static_cast<uint32_t>(std::clamp(interactionGridCount_, 1, 10));
			settings.gridCenter = interactionGridCenter_;
			settings.particleSize = std::clamp(interactionParticleSize_, 0.02f, 0.05f);
			settings.gridCountX = gridCount;
			settings.gridCountY = gridCount;
			settings.gridCountZ = gridCount;
			settings.particleCount = CalculateInteractionParticleCount();
			settings.brushPosition = interactionBrushPosition_;
			settings.brushRadius = (std::max)(interactionBrushRadius_, 0.01f);
			settings.brushStrength = (std::max)(interactionBrushStrength_, 0.0f);
			settings.operation = static_cast<uint32_t>(interactionBrushOperation_);
			settings.isPressed = settings.operation != static_cast<uint32_t>(InteractionBrushOperation::None) ? 1u : 0u;
			settings.deltaTime = sceneDeltaTime_;
			settings.damping = std::clamp(interactionDamping_, 0.80f, 0.995f);
			if (interactionResetRequested_) {
				particleManager->InitializeGPUParticleInteraction(settings);
				interactionResetRequested_ = false;
			}
			particleManager->UpdateGPUParticleInteraction(settings);
			particleManager->DrawGPUParticleBuffer();
		} else {
			particleManager->Draw();
		}
	}
}

void GamePlayScene::UpdateSceneDeltaTime() {
	const auto now = std::chrono::steady_clock::now();
	if (previousFrameTime_.time_since_epoch().count() != 0) {
		sceneDeltaTime_ = std::chrono::duration<float>(now - previousFrameTime_).count();
		sceneDeltaTime_ = std::clamp(sceneDeltaTime_, 1.0f / 240.0f, 1.0f / 15.0f);
	} else {
		sceneDeltaTime_ = 1.0f / 60.0f;
	}
	previousFrameTime_ = now;
}

void GamePlayScene::ResetCamera() {
	cameraPos_ = { 0.0f, 5.0f, -15.0f };
	cameraRot_ = { 0.3f, 0.0f, 0.0f };
}

void GamePlayScene::ClampCameraPitch() {
	cameraRot_.x = std::clamp(cameraRot_.x, -1.45f, 1.10f);
}

void GamePlayScene::HandleCameraInput(float deltaTime) {
	if (!framework_ || !viewportHovered_ || ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return;
	}

	Input* input = framework_->GetInput();
	if (!input) {
		return;
	}

	const Matrix4x4 rotateMatrix = MatrixMath::Multiply(
		MatrixMath::MakeRotateXMatrix(cameraRot_.x),
		MatrixMath::Multiply(
			MatrixMath::MakeRotateYMatrix(cameraRot_.y),
			MatrixMath::MakeRotateZMatrix(cameraRot_.z)));

	Vector3 forward = { rotateMatrix.m[2][0], rotateMatrix.m[2][1], rotateMatrix.m[2][2] };
	forward.y = 0.0f;
	if (MatrixMath::Length(forward) <= 0.0001f) {
		forward = { std::sin(cameraRot_.y), 0.0f, std::cos(cameraRot_.y) };
	}
	forward = MatrixMath::Normalize(forward);
	Vector3 right = MatrixMath::Normalize(MatrixMath::Cross({ 0.0f, 1.0f, 0.0f }, forward));

	Vector3 move = { 0.0f, 0.0f, 0.0f };
	if (input->PushKey(DIK_W)) { move += forward; }
	if (input->PushKey(DIK_S)) { move -= forward; }
	if (input->PushKey(DIK_D)) { move += right; }
	if (input->PushKey(DIK_A)) { move -= right; }
	if (input->PushKey(DIK_Q)) { move.y += 1.0f; }
	if (input->PushKey(DIK_E)) { move.y -= 1.0f; }

	const float moveLength = MatrixMath::Length(move);
	if (moveLength > 0.0001f) {
		const float moveStep = (std::max)(cameraMoveSpeed_, 0.0f) * deltaTime;
		cameraPos_ += (move / moveLength) * moveStep;
		cameraPos_.y = std::clamp(cameraPos_.y, 1.0f, 30.0f);
	}

	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
		const float rotateStep = (std::max)(cameraRotateSpeed_, 0.0f) * 0.01f;
		cameraRot_.y += mouseDelta.x * rotateStep;
		cameraRot_.x += mouseDelta.y * rotateStep;
	}
	ClampCameraPitch();
}

void GamePlayScene::HandleKeyboardMovement() {
	if (selectedTarget_ == 0 || viewportHovered_ || ImGui::GetIO().WantCaptureKeyboard) return;
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
void GamePlayScene::HandleFieldMouseSelection() {
	fieldMouseInViewport_ = false;
	fieldMouseRayValid_ = false;
	fieldMouseHit_ = false;
	fieldMouseVirtualPosition_ = { -1.0f, -1.0f };
	fieldMouseSelectedIndex_ = fieldManager_ ? fieldManager_->GetSelectedIndex() : -1;

	if (!fieldManager_ || !camera_ || gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
		return;
	}

	Vector2 virtualPosition{};
	fieldMouseInViewport_ = ConvertMouseToVirtualScreen(viewportMousePosition_, virtualPosition);
	if (!fieldMouseInViewport_) {
		return;
	}
	fieldMouseVirtualPosition_ = virtualPosition;

	Ray ray{};
	fieldMouseRayValid_ = CreateRayFromVirtualScreen(virtualPosition, ray);
	fieldMouseRayOrigin_ = ray.origin;
	fieldMouseRayDirection_ = ray.direction;
	if (!fieldMouseRayValid_) {
		return;
	}

	Vector3 hitPosition{};
	fieldMouseHit_ = IntersectRayPlaneY(ray, fieldManager_->GetGroundY(), hitPosition);
	if (!fieldMouseHit_) {
		return;
	}
	fieldMouseHitPosition_ = hitPosition;

	if (viewportHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		if (fieldManager_->TrySelectTileByWorldPosition(hitPosition)) {
			fieldMouseSelectedIndex_ = fieldManager_->GetSelectedIndex();
		}
	}
}

bool GamePlayScene::ConvertMouseToVirtualScreen(const Vector2& mouseScreenPos, Vector2& outVirtualPos) const {
	outVirtualPos = { -1.0f, -1.0f };
	if (viewportImageSize_.x <= 1.0f || viewportImageSize_.y <= 1.0f) {
		return false;
	}

	const float localX = mouseScreenPos.x - viewportImageTopLeft_.x;
	const float localY = mouseScreenPos.y - viewportImageTopLeft_.y;
	if (localX < 0.0f || localY < 0.0f ||
		localX > viewportImageSize_.x || localY > viewportImageSize_.y) {
		return false;
	}

	outVirtualPos.x = localX / viewportImageSize_.x * kVirtualScreenWidth;
	outVirtualPos.y = localY / viewportImageSize_.y * kVirtualScreenHeight;
	return std::isfinite(outVirtualPos.x) && std::isfinite(outVirtualPos.y);
}

bool GamePlayScene::CreateRayFromVirtualScreen(const Vector2& virtualScreenPos, Ray& outRay) const {
	outRay = {};
	if (!camera_ ||
		!std::isfinite(virtualScreenPos.x) ||
		!std::isfinite(virtualScreenPos.y)) {
		return false;
	}

	const float ndcX = (virtualScreenPos.x / kVirtualScreenWidth) * 2.0f - 1.0f;
	const float ndcY = 1.0f - (virtualScreenPos.y / kVirtualScreenHeight) * 2.0f;
	const Matrix4x4 inverseViewProjection = MatrixMath::Inverse(camera_->GetViewProjectionMatrix());
	const Vector3 nearPoint = MatrixMath::Transform({ ndcX, ndcY, 0.0f }, inverseViewProjection);
	const Vector3 farPoint = MatrixMath::Transform({ ndcX, ndcY, 1.0f }, inverseViewProjection);
	const Vector3 direction = MatrixMath::Normalize(farPoint - nearPoint);
	if (MatrixMath::Length(direction) <= 0.0001f ||
		!std::isfinite(nearPoint.x) ||
		!std::isfinite(nearPoint.y) ||
		!std::isfinite(nearPoint.z) ||
		!std::isfinite(direction.x) ||
		!std::isfinite(direction.y) ||
		!std::isfinite(direction.z)) {
		return false;
	}

	outRay.origin = nearPoint;
	outRay.direction = direction;
	return true;
}

bool GamePlayScene::IntersectRayPlaneY(const Ray& ray, float planeY, Vector3& outHitPosition) const {
	outHitPosition = {};
	if (std::abs(ray.direction.y) <= 0.0001f || !std::isfinite(planeY)) {
		return false;
	}

	const float t = (planeY - ray.origin.y) / ray.direction.y;
	if (t < 0.0f || !std::isfinite(t)) {
		return false;
	}

	outHitPosition = ray.origin + ray.direction * t;
	return
		std::isfinite(outHitPosition.x) &&
		std::isfinite(outHitPosition.y) &&
		std::isfinite(outHitPosition.z);
}

void GamePlayScene::SyncGPUParticleDebugModeChange() {
	if (gpuParticleDebugMode_ == previousGPUParticleDebugMode_) {
		return;
	}

	if (framework_ && framework_->GetParticleManager()) {
		framework_->GetParticleManager()->ResetGPUParticles();
	}
	if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
		interactionResetRequested_ = true;
		interactionBrushOperation_ = InteractionBrushOperation::None;
	}
	previousGPUParticleDebugMode_ = gpuParticleDebugMode_;
}

void GamePlayScene::SetGPUParticleDebugMode(GPUParticleDebugMode mode) {
	gpuParticleDebugMode_ = mode;
}

void GamePlayScene::HandleGPUParticleDebugModeInput() {
	switch (gpuParticleDebugMode_) {
	case GPUParticleDebugMode::Agriculture:
		HandleAgricultureParticleInput();
		break;
	case GPUParticleDebugMode::Interaction:
		HandleInteractionParticleInput();
		break;
	case GPUParticleDebugMode::Off:
	default:
		break;
	}
}

void GamePlayScene::HandleAgricultureParticleInput() {
	if (ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}

	Input* input = framework_->GetInput();
	if (input->TriggerKey(DIK_1)) {
		EmitAgricultureParticle(AgricultureParticleType::DirtDust);
	}
	if (input->TriggerKey(DIK_2)) {
		EmitAgricultureParticle(AgricultureParticleType::WaterSplash);
	}
	if (input->TriggerKey(DIK_3)) {
		EmitAgricultureParticle(AgricultureParticleType::HarvestSparkle);
	}
	if (input->TriggerKey(DIK_4)) {
		EmitAgricultureParticle(AgricultureParticleType::PollenSpore);
	}
	if (input->TriggerKey(DIK_5)) {
		EmitAgricultureParticle(AgricultureParticleType::BugSwarm);
	}
}

void GamePlayScene::HandleInteractionParticleInput() {
	interactionGridCount_ = std::clamp(interactionGridCount_, 1, 10);
	interactionParticleSize_ = std::clamp(interactionParticleSize_, 0.02f, 0.05f);
	interactionBrushRadius_ = (std::max)(interactionBrushRadius_, 0.01f);
	interactionBrushStrength_ = (std::max)(interactionBrushStrength_, 0.0f);
	interactionDamping_ = std::clamp(interactionDamping_, 0.80f, 0.995f);
	interactionParticleCount_ = CalculateInteractionParticleCount();

	Vector3 brushPosition{};
	const bool hasBrushPosition = TryGetInteractionBrushPosition(brushPosition);
	if (hasBrushPosition) {
		interactionBrushPosition_ = brushPosition;
	}

	interactionBrushOperation_ = InteractionBrushOperation::None;
	if (hasBrushPosition && viewportHovered_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		interactionBrushOperation_ = ImGui::GetIO().KeyShift
			? InteractionBrushOperation::Pull
			: InteractionBrushOperation::Push;
	}
}

void GamePlayScene::EmitAgricultureParticle(AgricultureParticleType type) {
	if (gpuParticleDebugMode_ != GPUParticleDebugMode::Agriculture) {
		return;
	}

	const uint32_t count = static_cast<uint32_t>(std::clamp(agricultureParticleCount_, 1, 1024));
	const float particleSize = (std::max)(agricultureParticleSize_, 0.01f);
	GPUParticleEmitSettings settings = MakeAgricultureEmitSettings(
		agricultureEmitPosition_,
		particleSize,
		count,
		type);
	framework_->GetParticleManager()->RequestGPUParticleEmit(settings);
}

uint32_t GamePlayScene::CalculateInteractionParticleCount() const {
	const uint32_t gridCount = static_cast<uint32_t>(std::clamp(interactionGridCount_, 1, 10));
	const uint64_t particleCount =
		static_cast<uint64_t>(gridCount) *
		static_cast<uint64_t>(gridCount) *
		static_cast<uint64_t>(gridCount);
	return static_cast<uint32_t>(particleCount > 1024 ? 1024 : particleCount);
}

bool GamePlayScene::TryGetInteractionBrushPosition(Vector3& outBrushPosition) const {
	if (!camera_ || !viewportHovered_ || viewportImageSize_.x <= 0.0f || viewportImageSize_.y <= 0.0f) {
		return false;
	}

	const float u = (viewportMousePosition_.x - viewportImageTopLeft_.x) / viewportImageSize_.x;
	const float v = (viewportMousePosition_.y - viewportImageTopLeft_.y) / viewportImageSize_.y;
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
		return false;
	}

	const float ndcX = u * 2.0f - 1.0f;
	const float ndcY = 1.0f - v * 2.0f;
	const Matrix4x4 inverseViewProjection = MatrixMath::Inverse(camera_->GetViewProjectionMatrix());
	const Vector3 nearPoint = MatrixMath::Transform({ ndcX, ndcY, 0.0f }, inverseViewProjection);
	const Vector3 farPoint = MatrixMath::Transform({ ndcX, ndcY, 1.0f }, inverseViewProjection);
	const Vector3 rayDirection = MatrixMath::Normalize(farPoint - nearPoint);
	if (std::abs(rayDirection.y) <= 0.0001f) {
		return false;
	}

	const float t = (interactionGridCenter_.y - nearPoint.y) / rayDirection.y;
	if (t < 0.0f || !std::isfinite(t)) {
		return false;
	}

	outBrushPosition = nearPoint + rayDirection * t;
	return
		std::isfinite(outBrushPosition.x) &&
		std::isfinite(outBrushPosition.y) &&
		std::isfinite(outBrushPosition.z);
}

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

/**
 * ChangeAnimationModel: インデックスに応じて再生するアニメーションモデルを動的に切り替える
 * 各モデル固有のサイズ（スケール）や初期位置の自動調整もここで行います
 */
void GamePlayScene::ChangeAnimationModel(int index) {
	std::vector<std::pair<std::string, std::string>> animModels = {
		{ "AnimatedCube", "AnimatedCube.gltf" },
		{ "simpleSkin", "simpleSkin.gltf" },
		{ "human", "walk.gltf" },
		{ "human", "sneakWalk.gltf" }
	};

	if (index < 0 || index >= static_cast<int>(animModels.size())) return;

	std::string path = animModels[index].first + "/" + animModels[index].second;
	Model* animModel = framework_->GetModelManager()->GetModel(path);
	if (animModel) {
		if (!animObj_) {
			animObj_ = std::make_unique<Object3d>();
			animObj_->Initialize(framework_->GetObject3dCommon());
		}
		// オブジェクトにモデルをセット
		animObj_->SetModel(animModel);
		
		// スケールと座標の調整 (モデルごとに適した表示サイズと位置へ自動的にアジャスト)
		if (index == 0) { // AnimatedCube (仮キューブモデル)
			animObj_->SetPosition({ 5.0f, 0.0f, 0.0f });
			animObj_->SetScale({ 1.0f, 1.0f, 1.0f });
		} else if (index == 1) { // simpleSkin (シンプルな関節スキン)
			animObj_->SetPosition({ 0.0f, 0.0f, 0.0f });
			animObj_->SetScale({ 0.5f, 0.5f, 0.5f }); // 画面に収まるように少し小さめにする
		} else { // human (人間モデル)
			animObj_->SetPosition({ 0.0f, -2.0f, 0.0f }); // 地面(Y=-2.0)にピッタリ乗るように設定
			animObj_->SetScale({ 1.0f, 1.0f, 1.0f });
		}
		
		// 環境マップ設定の引き継ぎ
		animObj_->SetEnvironmentMap(skyboxManager_ ? skyboxManager_->GetCurrentSrvIndex() : 0);
		
		// アニメーションデータをアセットフォルダから読み込んでオブジェクトにセット
		Animation anim = animModel->LoadAnimation("Resources/" + animModels[index].first, animModels[index].second);
		animObj_->SetAnimation(anim);
		
		// 再生制御パラメータの初期化
		animObj_->GetAnimationTime() = 0.0f;
		animObj_->GetIsAnimationPlaying() = true;
	}
}
