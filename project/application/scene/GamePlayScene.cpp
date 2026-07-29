#include "scene/GamePlayScene.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
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
#include "3d/LightingSystem.h"
#include "Game.h"
#include "3d/PrimitiveGenerator.h"
#include "3d/LineDrawer.h"
#include "level/LevelLoader.h"
#include <dinput.h>
#include "3d/Skybox.h"
#include "2d/TextureManager.h"
#include "base/Logger.h" // 追加：外部ロガーツールのインクルード
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <random>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace {
constexpr float kMouseLookSensitivity = 0.005f;
constexpr float kMousePanSensitivity = 0.01f;
constexpr float kMouseWheelZoomSpeed = 2.0f;
constexpr float kEditorCameraFastMultiplier = 3.0f;
constexpr std::size_t kTimelineSnapshotCapacity = 60u * 10u;
constexpr float kTimelineScrubAudioRate = 0.55f;
constexpr float kTimelineAudioEnterTransitionSeconds = 0.08f;
constexpr float kTimelineAudioExitTransitionSeconds = 0.75f;
constexpr const char* kRostockSkyboxPath = "Resources/rostock_laage_airport_4k.dds";
constexpr const char* kSceneLevelName = "scene";
constexpr const char* kMeshTypeName = "MESH";
constexpr const char* kCameraTypeName = "CAMERA";
constexpr const char* kLightTypeName = "LIGHT";
constexpr const char* kSpawnPointTypeName = "SPAWN_POINT";
constexpr const char* kEventTriggerTypeName = "EVENT_TRIGGER";
constexpr const char* kGimmickTypeName = "GIMMICK";
constexpr const char* kPatrolPointTypeName = "PATROL_POINT";
constexpr const char* kPlayerRoleName = "PLAYER";
constexpr const char* kCollectibleRoleName = "COLLECTIBLE";
constexpr const char* kGroundRoleName = "GROUND";
constexpr Vector3 kPlayerCameraOffset = { 0.0f, 3.5f, -8.0f };
constexpr Vector3 kPlayerCameraLookOffset = { 0.0f, 0.7f, 0.0f };
constexpr Vector3 kDefaultPlayerColliderCenterOffset = { 0.0f, 0.9f, 0.0f };
constexpr Vector3 kDefaultPlayerColliderHalfExtents = { 0.45f, 0.9f, 0.45f };
constexpr float kColliderCenterEpsilon = 0.0001f;
constexpr float kPlayerAnimationStopBlendSeconds = 0.20f;
#ifdef USE_IMGUI
constexpr Transform kDefaultBusterSwordTransform = {
	{ 1.0f, 1.0f, 1.0f },
	{ 0.0f, 0.0f, 1.5707963f },
	{ 0.0f, 0.0f, 0.0f },
};
#endif

struct AnimationModelDefinition {
	const char* directory;
	const char* fileName;
	const char* evaluationCacheKey;
};

constexpr std::array<AnimationModelDefinition, 4> kAnimationModels = {
	AnimationModelDefinition{ "AnimatedCube", "AnimatedCube.gltf", "AnimatedCube/AnimatedCube.gltf" },
	AnimationModelDefinition{ "simpleSkin", "simpleSkin.gltf", "simpleSkin/simpleSkin.gltf" },
	AnimationModelDefinition{ "human", "walk.gltf", "__cg4/human/walk.gltf" },
	AnimationModelDefinition{ "human", "sneakWalk.gltf", "__cg4/human/sneakWalk.gltf" },
};

Vector4 GetLevelRoleColor(const std::string& gameplayRole)
{
	if (gameplayRole == kPlayerRoleName) {
		return { 0.15f, 0.75f, 1.0f, 1.0f };
	}
	if (gameplayRole == kCollectibleRoleName) {
		return { 1.0f, 0.82f, 0.12f, 1.0f };
	}
	if (gameplayRole == kGroundRoleName) {
		return { 0.22f, 0.62f, 0.28f, 1.0f };
	}
	return { 0.85f, 0.85f, 0.85f, 1.0f };
}

float GetLevelObjectRadius(const Vector3& scale)
{
	return (std::max)(0.5f, 0.5f * (std::max)({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) }));
}

Vector3 AddVector3(const Vector3& a, const Vector3& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
{
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
	};
}

float MoveTowards(float current, float target, float maxDelta)
{
	if (current < target) {
		return (std::min)(current + maxDelta, target);
	}
	return (std::max)(current - maxDelta, target);
}

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

bool FileExistsNoThrow(const std::filesystem::path& path)
{
	std::error_code error;
	return std::filesystem::exists(path, error) && !error;
}

std::string BuildSelectedTileInfo(const farm::FarmGrid& grid)
{
	const farm::FarmTile* selectedTile = grid.GetSelectedTile();
	if (selectedTile == nullptr) {
		return "Tile Invalid";
	}

	const float clampedMoisture = std::clamp(selectedTile->moisture, 0.0f, 1.0f);
	const int moisturePercent = static_cast<int>(clampedMoisture * 100.0f + 0.5f);
	return "Tile " + std::to_string(grid.GetSelectedIndex()) +
		" H" + std::to_string(selectedTile->heightLevel) +
		" " + farm::ToString(selectedTile->state) +
		" Water " + std::to_string(moisturePercent) + "%" +
		" Crop " + farm::ToString(selectedTile->crop);
}

void BuildCameraGroundMoveAxes(float yaw, Vector3& right, Vector3& forward)
{
	const float sinYaw = std::sin(yaw);
	const float cosYaw = std::cos(yaw);

	right = { cosYaw, 0.0f, -sinYaw };
	forward = { sinYaw, 0.0f, cosYaw };
}

void BuildCameraViewAxes(const Vector3& rotate, Vector3& right, Vector3& up, Vector3& forward)
{
	const Matrix4x4 rotateMatrix = MatrixMath::Multiply(
		MatrixMath::MakeRotateXMatrix(rotate.x),
		MatrixMath::Multiply(
			MatrixMath::MakeRotateYMatrix(rotate.y),
			MatrixMath::MakeRotateZMatrix(rotate.z)));

	right = MatrixMath::Normalize({ rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2] });
	up = MatrixMath::Normalize({ rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2] });
	forward = MatrixMath::Normalize({ rotateMatrix.m[2][0], rotateMatrix.m[2][1], rotateMatrix.m[2][2] });
}

std::string ResolveModelFilename(const std::string& fileName)
{
	const std::filesystem::path resourceRoot = "Resources";
	const std::filesystem::path sourcePath = fileName;

	if (sourcePath.has_extension() && FileExistsNoThrow(resourceRoot / sourcePath)) {
		return fileName;
	}

	const std::filesystem::path parentPath = sourcePath.parent_path();
	const std::string stem = sourcePath.filename().string();
	const std::filesystem::path relativeStemPath = parentPath / stem;
	const std::filesystem::path folderStemPath = sourcePath / stem;
	const std::filesystem::path candidates[] = {
		relativeStemPath.string() + ".obj",
		relativeStemPath.string() + ".gltf",
		relativeStemPath.string() + ".fbx",
		folderStemPath.string() + ".obj",
		folderStemPath.string() + ".gltf",
		folderStemPath.string() + ".fbx",
	};

	for (const std::filesystem::path& candidate : candidates) {
		if (FileExistsNoThrow(resourceRoot / candidate)) {
			return candidate.generic_string();
		}
	}

	return fileName;
}

}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

/**
 * Initialize: シーン開始時に一度だけ呼ばれるセットアップ関数
 */
void GamePlayScene::Initialize() {
	framework_ = Framework::GetInstance();
	if (framework_ && framework_->GetAudio()) {
		framework_->GetAudio()->SetGlobalTemporalState(
			AudioPlaybackDirection::Forward,
			1.0f,
			0.0f);
	}
	farmGrid_.Initialize(5, 4);
	farmDateSystem_.Initialize();
	farmToolSystem_.Initialize();
#ifdef USE_IMGUI
	farmDebugEditorWindow_.LoadSettings();
#endif

	// ログ記録：UIと外部出力の両方に行われます
	AddLog("Scene: GamePlay Initialized.");

	// ---------------------------------------------------------
	// 1. テクスチャ・リソースの読み込み
	// ---------------------------------------------------------
	textureHandles_.clear();
	std::vector<std::string> textureNames = { "monsterBall.png", "uvChecker.png", "choju8_0008.png" };
	for (const auto& name : textureNames) {
		textureHandles_.push_back(TextureManager::GetInstance()->LoadTexture2D("Resources/" + name));
	}
	levelWhiteTextureHandle_ = TextureManager::GetInstance()->LoadTexture2D("Resources/human/white.png");
	Texture2DHandle circle2Handle = TextureManager::GetInstance()->LoadTexture2D("Resources/circle2.png");
	ringTexHandle_ = TextureManager::GetInstance()->LoadTexture2D("Resources/gradationLine.png");

	// ---------------------------------------------------------
	// 2. システム・環境の初期化
	// ---------------------------------------------------------
	camera_ = std::make_unique<Camera>();
	LineDrawer::GetInstance()->Initialize(framework_->GetDxCommon());

	skeletonDebugger_ = std::make_unique<SkeletonDebugger>();
	skeletonDebugger_->Initialize(framework_->GetObject3dCommon(), framework_->GetModelManager());

	// ---------------------------------------------------------
	// 3. モデルデータのロード
	// ---------------------------------------------------------
	std::string terrainPath = "terrain/terrain.obj";
	framework_->GetModelManager()->LoadModel(terrainPath);
	Model* tModel = framework_->GetModelManager()->GetModel(terrainPath);
	if (tModel) { tModel->LoadTextures(); }

	framework_->GetModelManager()->LoadModel("plane.obj");
	Model* planeModel = framework_->GetModelManager()->GetModel("plane.obj");

	// Level Playerと評価用Humanがmutable SkinClusterを共有しないよう、
	// human ClipだけはUSE_IMGUI構成で専用Model instanceもプリロードする。
	for (const AnimationModelDefinition& definition : kAnimationModels) {
		const std::string path = std::string(definition.directory) + "/" + definition.fileName;
		framework_->GetModelManager()->LoadModel(path);
		Model* m = framework_->GetModelManager()->GetModel(path);
		if (m) {
			m->LoadTextures();
		}
#ifdef USE_IMGUI
		if (path != definition.evaluationCacheKey) {
			framework_->GetModelManager()->LoadModelAs(definition.evaluationCacheKey, path);
			if (Model* evaluationModel = framework_->GetModelManager()->GetModel(definition.evaluationCacheKey)) {
				evaluationModel->LoadTextures();
			}
		}
#endif
	}

	constexpr const char* kMultiMeshMaterialSamplePath = "multiMaterial.obj";
	framework_->GetModelManager()->LoadModel(kMultiMeshMaterialSamplePath);
	Model* multiMeshMaterialModel = framework_->GetModelManager()->GetModel(kMultiMeshMaterialSamplePath);
	if (multiMeshMaterialModel) {
		multiMeshMaterialModel->LoadTextures();
	}

#ifdef USE_IMGUI
	constexpr const char* kBusterSwordModelPath = "バスターソード/BusterSword.obj";
	framework_->GetModelManager()->LoadModel(kBusterSwordModelPath);
	Model* busterSwordModel = framework_->GetModelManager()->GetModel(kBusterSwordModelPath);
	if (busterSwordModel) {
		busterSwordModel->LoadTextures();
	}
#endif

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

	multiMeshMaterialObj_ = std::make_unique<Object3d>();
	multiMeshMaterialObj_->Initialize(framework_->GetObject3dCommon());
	multiMeshMaterialObj_->SetModel(multiMeshMaterialModel);
	multiMeshMaterialObj_->SetPosition({ -1.25f, -0.5f, 0.0f });

#ifdef USE_IMGUI
	busterSwordObj_ = std::make_unique<Object3d>();
	busterSwordObj_->Initialize(framework_->GetObject3dCommon());
	busterSwordObj_->SetModel(busterSwordModel);
	busterSwordObj_->SetCullMode(0);
	busterSwordObj_->SetEnableLighting(false);
#endif

	// The human animation is assigned to the level PLAYER object, not shown as a separate sample object.
	showAnimModel_ = false;
	LoadSceneLevel();

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(framework_->GetSpriteCommon(), "Resources/" + textureNames[0]);

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

	InitializeFarmHUD();

#ifdef USE_IMGUI
	CG4EvaluationActions initialEvaluationActions;
	initialEvaluationActions.preset = CG4EvaluationPreset::WeaponAttachment;
	ApplyCG4EvaluationActions(initialEvaluationActions);
#endif

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

#ifndef USE_IMGUI
	// A production build receives mouse coordinates from the game window because no editor viewport exists.
	viewportHovered_ = true;
	viewportImageTopLeft_ = { 0.0f, 0.0f };
	viewportImageSize_ = {
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight)
	};
	if (framework_ && framework_->GetInput()) {
		viewportMousePosition_ = framework_->GetInput()->GetMousePosition();
	}
#endif

	Input* frameInput = framework_ ? framework_->GetInput() : nullptr;
	const bool controlHeld = frameInput && (frameInput->PushKey(DIK_LCONTROL) || frameInput->PushKey(DIK_RCONTROL));
	bool levelReloadRequested = frameInput &&
		(frameInput->TriggerKey(DIK_F5) || (controlHeld && frameInput->TriggerKey(DIK_R)));
	bool cameraModeToggleRequested = framework_ && framework_->GetInput() && framework_->GetInput()->TriggerKey(DIK_F1);
	if (ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		levelReloadRequested = false;
		cameraModeToggleRequested = false;
	}
	if (levelReloadRequested) {
		LoadSceneLevel();
	}
	if (cameraModeToggleRequested) {
		ToggleCameraMode();
	}

	const bool farmGridInputConsumed = HandleFarmGridSelectionInput();
	HandleCameraInput(sceneDeltaTime_, farmGridInputConsumed);
	ClampCameraPitch();
	if (!farmGridInputConsumed) {
		HandleKeyboardMovement();
	}
	SyncLevelGameplayPresentation();
	UpdatePlayerCamera();
	HandleFarmHistoryInput();
	HandleFarmDateDebugInput();
	HandleFarmToolDebugInput();
	HandleFarmToolActionInput();

	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);
	camera_->Update();

	sprite_->SetPosition(spritePos_);
	sprite_->Update();
	if (farmHudInitialized_ && showFarmHud_) {
		farmHud_.SetViewData(BuildFarmHUDViewData());
		farmHud_.Update(sceneDeltaTime_);
	}
#ifdef USE_IMGUI
	if (showLegacySceneDebugWindows_) {
		farmDebugEditorWindow_.Draw(farmGrid_, farmToolActionSystem_);
		DrawSceneDebugWindow();
	}
#endif

	if (skyboxEnabled_) {
		InitializeSkyboxIfNeeded();
		if (skybox_) {
			skybox_->Update(camera_.get());
		}
	}

	LightingSystem* lightingSystem = framework_->GetLightingSystem();
	assert(lightingSystem != nullptr);
	lightingSystem->SetDirectionalLight({
		{ lightColor_.x, lightColor_.y, lightColor_.z, 1.0f },
		lightDirection_,
		lightIntensity_ });
	LightingSystem::SpotLight spotLight{};
	spotLight.color = { spotLightColor_.x, spotLightColor_.y, spotLightColor_.z, 1.0f };
	spotLight.position = spotLightPos_;
	spotLight.intensity = spotLightIntensity_;
	spotLight.direction = spotLightDir_;
	spotLight.distance = spotLightDistance_;
	spotLight.decay = spotLightDecay_;
	spotLight.cosAngle = std::cos(spotLightAngle_);
	spotLight.cosFalloffStart = std::cos(spotLightFalloff_);
	lightingSystem->SetSpotLight(spotLight);
	lightingSystem->SetCameraPosition(camera_->GetTranslate());

	auto UpdateObjectState = [&](Object3d* obj, float envCoef) {
		if (!obj) return;
		obj->SetCullMode(cullMode_);

		if (skybox_ && skyboxEnvironmentEnabled_) {
			obj->SetEnvironmentMap(skybox_->GetTextureHandle());
			obj->SetEnvironmentCoefficient(envCoef);
		} else {
			obj->SetEnvironmentCoefficient(0.0f);
		}

		obj->Update(camera_.get(), timelineScrubbing_ ? 0.0f : sceneDeltaTime_);
		};

	UpdateObjectState(terrainObj_.get(), 0.0f);
	UpdateObjectState(object3d_.get(), 0.0f);
	UpdateObjectState(animObj_.get(), 0.5f);
#ifdef USE_IMGUI
	busterSwordAttachment_.SetEnabled(showBusterSword_);
	const bool busterSwordResolved = busterSwordAttachment_.Update();
	if (showBusterSword_ && busterSwordResolved) {
		UpdateObjectState(busterSwordObj_.get(), 0.25f);
		busterSwordObj_->SetCullMode(0);
	}
#endif
	if (showMultiMeshMaterialSample_) {
		UpdateObjectState(multiMeshMaterialObj_.get(), 0.0f);
	}
	for (LevelObjectRuntime& levelObject : levelObjects_) {
		if (!levelObject.object || !levelObject.visible) {
			continue;
		}
		if (levelObject.gameplayRole == kCollectibleRoleName) {
			const float animationTime = levelRouteTimer_ * 2.0f + levelObject.animationPhase;
			Vector3 position = levelObject.basePosition;
			position.y += std::sin(animationTime) * 0.2f;
			levelObject.object->SetPosition(position);
			levelObject.object->SetRotation({ 0.0f, animationTime, 0.0f });
		}
		UpdateObjectState(levelObject.object.get(), 0.0f);
	}

	if (sphereObj_) {
		sphereObj_->SetPosition(spherePos_);
		sphereObj_->SetRotation(objectRot_);
		UpdateObjectState(sphereObj_.get(), 0.5f);
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

	framework_->GetParticleManager()->Update(camera_.get(), timelineScrubbing_ ? 0.0f : sceneDeltaTime_);
}

void GamePlayScene::FixedUpdate(float fixedDeltaTime) {
	if (timelineScrubbing_) {
		const bool stepped = timelineForwardHeld_
			? timeline_.StepForward(timelineScratch_)
			: timeline_.StepBackward(timelineScratch_);
		if (stepped) {
			RestoreTimelineSnapshot(timelineScratch_);
		}
		timelineStepBackwardRequested_ = false;
		timelineStepForwardRequested_ = false;
		return;
	}

	levelRouteTimer_ += fixedDeltaTime;
	farmDateSystem_.Update(fixedDeltaTime);
	levelGameplay_.UpdatePlayer(pendingPlayerCommand_, fixedDeltaTime);
	pendingPlayerCommand_.jumpPressed = false;
	CaptureTimelineSnapshot(timelineScratch_);
	timeline_.Record(timelineScratch_);
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

void GamePlayScene::LoadSceneLevel() {
	levelData_ = level::LevelLoader::Load(kSceneLevelName);
	levelObjects_.clear();
	levelGameplay_.Reset();

	if (!levelData_) {
		timeline_.Clear();
		AddLog("Level load failed: Resources/levels/scene.json");
		return;
	}

	CollectLevelRuntimeData();
	ApplyLevelCamera();
	CreateLevelObjectsFromLevel();
	farmToolActionSystem_.ClearHistory();
	InitializeTimeline();
	AddLog("Level MESH objects: " + std::to_string(levelObjects_.size()));
}

void GamePlayScene::CreateLevelObjectsFromLevel() {
	levelObjects_.clear();
	levelGameplay_.Reset();
	playerVisualConfigured_ = false;
	playerAnimationState_ = PlayerAnimationState::Idle;
	playerAnimationSpeed_ = 0.0f;
	if (!framework_ || !levelData_) {
		return;
	}

	ModelManager* modelManager = framework_->GetModelManager();
	if (!modelManager) {
		AddLog("Level object creation failed: ModelManager is null.");
		return;
	}

	std::size_t meshCount = 0;
	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.type == kMeshTypeName && !objectData.disabled) {
			++meshCount;
		}
	}
	levelObjects_.reserve(meshCount);

	std::unordered_set<std::string> loadedModelNames;
	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.type != kMeshTypeName) {
			continue;
		}
		if (objectData.disabled) {
			AddLog("Level MESH disabled: " + objectData.name);
			continue;
		}
		if (objectData.fileName.empty()) {
			AddLog("Level MESH skipped. file_name is empty.");
			continue;
		}

		const std::string modelFileName = ResolveModelFilename(objectData.fileName);
		if (!FileExistsNoThrow(std::filesystem::path("Resources") / modelFileName)) {
			AddLog("Level MESH skipped. Model file not found: " + modelFileName);
			continue;
		}

		modelManager->LoadModel(modelFileName);
		Model* model = modelManager->GetModel(modelFileName);
		if (!model) {
			AddLog("Level MESH skipped. Model load failed: " + modelFileName);
			continue;
		}

		if (loadedModelNames.insert(modelFileName).second) {
			model->LoadTextures();
		}

		std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
		object->Initialize(framework_->GetObject3dCommon());
		object->SetModel(model);
		object->SetPosition(objectData.transform.translation);
		object->SetRotation(objectData.transform.rotation);
		object->SetScale(objectData.transform.scaling);
		object->SetTexture(levelWhiteTextureHandle_);
		object->SetColor(GetLevelRoleColor(objectData.gameplayRole));
		object->SetShininess(20.0f);

		const std::size_t renderIndex = levelObjects_.size();
		LevelObjectRuntime runtime{};
		runtime.object = std::move(object);
		runtime.name = objectData.name;
		runtime.gameplayRole = objectData.gameplayRole;
		runtime.basePosition = objectData.transform.translation;
		runtime.animationPhase = static_cast<float>(renderIndex) * 0.7f;
		levelObjects_.push_back(std::move(runtime));

		if (objectData.gameplayRole == kPlayerRoleName) {
			Vector3 colliderCenterOffset = kDefaultPlayerColliderCenterOffset;
			Vector3 colliderHalfExtents = kDefaultPlayerColliderHalfExtents;
			if (objectData.hasCollider && objectData.collider.type == "BOX") {
				colliderCenterOffset = objectData.collider.center;
				colliderHalfExtents = {
					std::abs(objectData.collider.size.x) * 0.5f,
					std::abs(objectData.collider.size.y) * 0.5f,
					std::abs(objectData.collider.size.z) * 0.5f,
				};
				if (std::abs(colliderCenterOffset.y) <= kColliderCenterEpsilon) {
					colliderCenterOffset.y = colliderHalfExtents.y;
				}
			}
			levelGameplay_.ConfigurePlayer(
				renderIndex,
				objectData.transform.translation,
				colliderCenterOffset,
				colliderHalfExtents);
		} else if (objectData.gameplayRole == kCollectibleRoleName) {
			levelGameplay_.AddCollectible(renderIndex, objectData.transform.translation, GetLevelObjectRadius(objectData.transform.scaling));
		} else if (objectData.gameplayRole == kGroundRoleName) {
			const Vector3& scale = objectData.transform.scaling;
			levelGameplay_.ConfigureGround(
				objectData.transform.translation,
				{ (std::max)(std::abs(scale.x), 0.5f), 0.05f, (std::max)(std::abs(scale.z), 0.5f) });
		} else {
			const Vector3& scale = objectData.transform.scaling;
			levelGameplay_.AddObstacle(
				objectData.transform.translation,
				{ (std::max)(std::abs(scale.x), 0.25f),
				  (std::max)(std::abs(scale.y), 0.25f),
				  (std::max)(std::abs(scale.z), 0.25f) });
		}
	}

	if (levelGameplay_.HasPlayer()) {
		const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
		if (playerIndex < levelObjects_.size() && levelObjects_[playerIndex].object) {
			levelObjects_[playerIndex].object->SetPosition(levelGameplay_.GetPlayerPosition());
		}
		UpdateLevelPlayerVisual();
		UpdatePlayerCamera();
	} else if (usePlayerCamera_ && hasLevelCamera_) {
		cameraPos_ = levelCameraPos_;
		cameraRot_ = levelCameraRot_;
		AddLog("PLAYER role not found. Using Blender camera as fallback.");
	}
}

void GamePlayScene::PrepareFixedUpdate()
{
	pendingPlayerCommand_.moveDirection = {};
	pendingPlayerCommand_.sneakHeld = false;
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	const bool keyboardCaptured = ImGuiManager::GetInstance()->WantsCaptureKeyboard();
	const bool controlHeld = input && (input->PushKey(DIK_LCONTROL) || input->PushKey(DIK_RCONTROL));
	const bool rewindKeyHeld = input && !keyboardCaptured && !controlHeld && input->PushKey(DIK_R);
	const bool shiftHeld = input && (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT));
	const bool wasScrubbing = timelineScrubbing_;
	timelineForwardHeld_ = (rewindKeyHeld && shiftHeld) || timelineStepForwardRequested_;
	timelineRewindHeld_ = (rewindKeyHeld && !shiftHeld) || timelineStepBackwardRequested_;
	timelineScrubbing_ = timeline_.IsInitialized() && (timelineRewindHeld_ || timelineForwardHeld_);
	if (framework_ && framework_->GetAudio()) {
		if (timelineScrubbing_) {
			framework_->GetAudio()->SetGlobalTemporalState(
				timelineForwardHeld_
					? AudioPlaybackDirection::Forward
					: AudioPlaybackDirection::Reverse,
				kTimelineScrubAudioRate,
				kTimelineAudioEnterTransitionSeconds);
		} else if (wasScrubbing) {
			framework_->GetAudio()->SetGlobalTemporalState(
				AudioPlaybackDirection::Forward,
				1.0f,
				kTimelineAudioExitTransitionSeconds);
		}
	}
	if (timelineScrubbing_ && !wasScrubbing) {
		farmToolActionSystem_.ClearHistory();
		if (framework_ && framework_->GetParticleManager()) {
			framework_->GetParticleManager()->ClearAll();
			framework_->GetParticleManager()->ResetGPUParticles();
		}
	}
	if (timelineScrubbing_) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}

	if (!framework_ || !levelGameplay_.HasPlayer() || keyboardCaptured) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}

	if (!input) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}
	if (!usePlayerCamera_ && input->PushMouseButton(InputMouseButton::Right)) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}

	if (input->PushKey(InputKey::W)) { pendingPlayerCommand_.moveDirection.z += 1.0f; }
	if (input->PushKey(InputKey::S)) { pendingPlayerCommand_.moveDirection.z -= 1.0f; }
	if (input->PushKey(InputKey::D)) { pendingPlayerCommand_.moveDirection.x += 1.0f; }
	if (input->PushKey(InputKey::A)) { pendingPlayerCommand_.moveDirection.x -= 1.0f; }
	pendingPlayerCommand_.jumpPressed = pendingPlayerCommand_.jumpPressed || input->TriggerKey(DIK_SPACE);
	pendingPlayerCommand_.sneakHeld =
		input->PushKey(InputKey::LeftShift) ||
		input->PushKey(InputKey::RightShift);
}

void GamePlayScene::SyncLevelGameplayPresentation()
{
	UpdateLevelPlayerVisual();

	for (const std::size_t renderIndex : levelGameplay_.ConsumeCollectedRenderIndices()) {
		if (renderIndex >= levelObjects_.size()) {
			continue;
		}
		LevelObjectRuntime& collectible = levelObjects_[renderIndex];
		if (!collectible.visible) {
			continue;
		}
		collectible.visible = false;
		EmitSpark(collectible.basePosition);
		AddLog("Collected: " + collectible.name);
	}
}

void GamePlayScene::InitializeTimeline()
{
	timeline_.Clear();
	CaptureTimelineSnapshot(timelineScratch_);
	timeline_.Initialize(kTimelineSnapshotCapacity, timelineScratch_);
	timelineRewindHeld_ = false;
	timelineForwardHeld_ = false;
	timelineScrubbing_ = false;
	timelineStepBackwardRequested_ = false;
	timelineStepForwardRequested_ = false;
}

void GamePlayScene::CaptureTimelineSnapshot(GameplaySnapshot& snapshot) const
{
	levelGameplay_.CaptureSnapshot(snapshot.levelGameplay);
	farmGrid_.CaptureSnapshot(snapshot.farmGrid);
	snapshot.farmDate = farmDateSystem_.CaptureSnapshot();
	snapshot.farmTool = farmToolSystem_.GetCurrentTool();
	snapshot.levelRouteTimer = levelRouteTimer_;
	snapshot.playerAnimationSpeed = playerAnimationSpeed_;
	snapshot.playerAnimationTime = 0.0f;
	if (levelGameplay_.HasPlayer()) {
		const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
		if (playerIndex < levelObjects_.size() && levelObjects_[playerIndex].object) {
			snapshot.playerAnimationTime = levelObjects_[playerIndex].object->GetAnimationTime();
		}
	}
}

bool GamePlayScene::RestoreTimelineSnapshot(const GameplaySnapshot& snapshot)
{
	if (!std::isfinite(snapshot.levelRouteTimer) || snapshot.levelRouteTimer < 0.0f ||
		!std::isfinite(snapshot.playerAnimationTime) || snapshot.playerAnimationTime < 0.0f ||
		!std::isfinite(snapshot.playerAnimationSpeed)) {
		return false;
	}
	if (!levelGameplay_.RestoreSnapshot(snapshot.levelGameplay) ||
		!farmGrid_.RestoreSnapshot(snapshot.farmGrid) ||
		!farmDateSystem_.RestoreSnapshot(snapshot.farmDate)) {
		return false;
	}

	farmToolSystem_.SetTool(snapshot.farmTool);
	levelRouteTimer_ = snapshot.levelRouteTimer;
	playerAnimationSpeed_ = snapshot.playerAnimationSpeed;
	pendingPlayerCommand_.jumpPressed = false;

	for (const auto& collectible : levelGameplay_.GetCollectibleColliders()) {
		if (collectible.renderIndex < levelObjects_.size()) {
			levelObjects_[collectible.renderIndex].visible = !collectible.collected;
		}
	}
	if (levelGameplay_.HasPlayer()) {
		const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
		if (playerIndex < levelObjects_.size() && levelObjects_[playerIndex].object) {
			Object3d* playerObject = levelObjects_[playerIndex].object.get();
			playerObject->SetPosition(levelGameplay_.GetPlayerPosition());
			playerObject->GetAnimationTime() = snapshot.playerAnimationTime;
			playerObject->GetAnimationSpeed() = snapshot.playerAnimationSpeed;
		}
	}
	return true;
}

void GamePlayScene::UpdateLevelPlayerVisual()
{
	if (!levelGameplay_.HasPlayer()) {
		return;
	}

	const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
	if (playerIndex >= levelObjects_.size() || !levelObjects_[playerIndex].object) {
		return;
	}

	if (!levelGameplay_.IsPlayerGrounded()) {
		playerAnimationState_ = PlayerAnimationState::Airborne;
	} else if (!levelGameplay_.IsPlayerMoving()) {
		playerAnimationState_ = PlayerAnimationState::Idle;
	} else if (levelGameplay_.IsPlayerSneaking()) {
		playerAnimationState_ = PlayerAnimationState::Sneak;
	} else {
		playerAnimationState_ = PlayerAnimationState::Walk;
	}

	PlayerAnimationMode desiredMode = playerAnimationMode_;
	if (playerAnimationState_ == PlayerAnimationState::Walk) {
		desiredMode = PlayerAnimationMode::Walk;
	} else if (playerAnimationState_ == PlayerAnimationState::Sneak) {
		desiredMode = PlayerAnimationMode::Sneak;
	}
	if (!playerVisualConfigured_ || desiredMode != playerAnimationMode_) {
		ConfigureLevelPlayerAnimation(playerIndex, desiredMode);
	}

	Object3d* playerObject = levelObjects_[playerIndex].object.get();
	playerObject->SetPosition(levelGameplay_.GetPlayerPosition());
	if (levelGameplay_.IsPlayerMoving()) {
		const Vector3& facing = levelGameplay_.GetPlayerFacingDirection();
		playerObject->SetRotation({ 0.0f, std::atan2(facing.x, facing.z), 0.0f });
	}
	const PlayerAnimationClip& clip = GetPlayerAnimationClip(playerAnimationMode_);
	float targetSpeed = clip.playbackSpeed;
	if (playerAnimationState_ == PlayerAnimationState::Idle) {
		targetSpeed = 0.0f;
	} else if (playerAnimationState_ == PlayerAnimationState::Airborne) {
		targetSpeed *= 0.35f;
	}
	const float maxSpeedChange = sceneDeltaTime_ / kPlayerAnimationStopBlendSeconds;
	playerAnimationSpeed_ = MoveTowards(playerAnimationSpeed_, targetSpeed, maxSpeedChange);
	playerObject->GetAnimationSpeed() = playerAnimationSpeed_;
	playerObject->GetIsAnimationPlaying() = playerAnimationSpeed_ > 0.001f;

}

const GamePlayScene::PlayerAnimationClip& GamePlayScene::GetPlayerAnimationClip(PlayerAnimationMode mode)
{
	static constexpr PlayerAnimationClip walk{ "walk.gltf", 0.65f };
	static constexpr PlayerAnimationClip sneak{ "sneakWalk.gltf", 0.45f };
	return mode == PlayerAnimationMode::Sneak ? sneak : walk;
}

void GamePlayScene::ConfigureLevelPlayerAnimation(std::size_t playerRenderIndex, PlayerAnimationMode mode)
{
	if (!framework_ || playerRenderIndex >= levelObjects_.size() || !levelObjects_[playerRenderIndex].object) {
		return;
	}

	const PlayerAnimationClip& clip = GetPlayerAnimationClip(mode);
	const char* animationFileName = clip.fileName;
	Model* playerModel = framework_->GetModelManager()->GetModel(std::string("human/") + animationFileName);
	if (!playerModel) {
		AddLog("Player model is not loaded: " + std::string(animationFileName));
		return;
	}

	Object3d* playerObject = levelObjects_[playerRenderIndex].object.get();
	float normalizedTime = 0.0f;
	if (playerVisualConfigured_ && playerObject->GetAnimation().duration > 0.0f) {
		normalizedTime = playerObject->GetAnimationTime() / playerObject->GetAnimation().duration;
	}
	playerObject->SetModel(playerModel);
	playerObject->SetTexture(levelWhiteTextureHandle_);
	playerObject->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	playerObject->SetScale({ 0.75f, 0.75f, 0.75f });
	playerObject->SetAnimation(playerModel->LoadAnimation("Resources/human", animationFileName));
	playerObject->GetAnimationTime() = playerObject->GetAnimation().duration * std::clamp(normalizedTime, 0.0f, 1.0f);
	playerObject->GetIsAnimationPlaying() = true;
	playerAnimationMode_ = mode;
	playerVisualConfigured_ = true;
	AddLog(std::string("Player animation: ") + (mode == PlayerAnimationMode::Sneak ? "sneak walk" : "walk"));
}

void GamePlayScene::CollectLevelRuntimeData() {
	levelRoutePoints_.clear();
	if (!levelData_) {
		return;
	}

	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.disabled) {
			continue;
		}
		if (objectData.type == kPatrolPointTypeName) {
			levelRoutePoints_.push_back(objectData.transform.translation);
		}
	}
}

void GamePlayScene::ApplyLevelCamera() {
	hasLevelCamera_ = false;
	if (!levelData_) {
		return;
	}

	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.disabled || objectData.type != kCameraTypeName) {
			continue;
		}

		levelCameraPos_ = objectData.transform.translation;
		levelCameraRot_ = objectData.transform.rotation;
		levelCameraRot_.x = std::clamp(levelCameraRot_.x, -1.5f, 1.5f);
		hasLevelCamera_ = true;
		AddLog("Blender camera loaded as fallback: " + objectData.name);
		return;
	}

	AddLog("Blender camera not found. PLAYER camera requires a PLAYER role.");
}

void GamePlayScene::DrawLevelBox(const Vector3& center, const Vector3& size, const Vector4& color) const {
	const Vector3 half = {
		size.x * 0.5f,
		size.y * 0.5f,
		size.z * 0.5f,
	};
	const Vector3 v[8] = {
		{ center.x - half.x, center.y - half.y, center.z - half.z },
		{ center.x + half.x, center.y - half.y, center.z - half.z },
		{ center.x + half.x, center.y + half.y, center.z - half.z },
		{ center.x - half.x, center.y + half.y, center.z - half.z },
		{ center.x - half.x, center.y - half.y, center.z + half.z },
		{ center.x + half.x, center.y - half.y, center.z + half.z },
		{ center.x + half.x, center.y + half.y, center.z + half.z },
		{ center.x - half.x, center.y + half.y, center.z + half.z },
	};
	const int edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	};

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	for (const auto& edge : edges) {
		lineDrawer->DrawLine(v[edge[0]], v[edge[1]], color);
	}
}

void GamePlayScene::DrawLevelCameraGizmo(const Vector3& position, const Vector4& color) const {
	const Vector3 nearCenter = AddVector3(position, { 0.0f, 0.0f, 0.6f });
	const Vector3 farCenter = AddVector3(position, { 0.0f, 0.0f, 1.8f });
	const Vector3 farCorners[4] = {
		{ farCenter.x - 0.8f, farCenter.y + 0.5f, farCenter.z },
		{ farCenter.x + 0.8f, farCenter.y + 0.5f, farCenter.z },
		{ farCenter.x + 0.8f, farCenter.y - 0.5f, farCenter.z },
		{ farCenter.x - 0.8f, farCenter.y - 0.5f, farCenter.z },
	};

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	lineDrawer->DrawWireSphere(position, 0.25f, color, 12);
	for (const Vector3& corner : farCorners) {
		lineDrawer->DrawLine(position, corner, color);
	}
	lineDrawer->DrawLine(farCorners[0], farCorners[1], color);
	lineDrawer->DrawLine(farCorners[1], farCorners[2], color);
	lineDrawer->DrawLine(farCorners[2], farCorners[3], color);
	lineDrawer->DrawLine(farCorners[3], farCorners[0], color);
	lineDrawer->DrawLine(position, nearCenter, color);
}

void GamePlayScene::DrawLevelCollisionGizmos() const {
	if (!showLevelCollisionGizmos_) {
		return;
	}

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	if (levelGameplay_.HasPlayer()) {
		const Vector3& half = levelGameplay_.GetPlayerColliderHalfExtents();
		DrawLevelBox(
			levelGameplay_.GetPlayerColliderCenter(),
			{ half.x * 2.0f, half.y * 2.0f, half.z * 2.0f },
			{ 0.1f, 1.0f, 1.0f, 1.0f });
	}
	if (levelGameplay_.HasGround()) {
		const Vector3& half = levelGameplay_.GetGroundHalfExtents();
		DrawLevelBox(levelGameplay_.GetGroundCenter(), { half.x * 2.0f, half.y * 2.0f, half.z * 2.0f }, { 0.2f, 1.0f, 0.25f, 1.0f });
	}
	for (const level::LevelGameplaySystem::ObstacleCollider& obstacle : levelGameplay_.GetObstacleColliders()) {
		DrawLevelBox(obstacle.center, { obstacle.halfExtents.x * 2.0f, obstacle.halfExtents.y * 2.0f, obstacle.halfExtents.z * 2.0f }, { 1.0f, 0.2f, 0.2f, 1.0f });
	}
	for (const level::LevelGameplaySystem::CollectibleCollider& collectible : levelGameplay_.GetCollectibleColliders()) {
		if (!collectible.collected) {
			lineDrawer->DrawWireSphere(collectible.position, collectible.radius, { 1.0f, 0.85f, 0.1f, 1.0f }, 16);
		}
	}
}

Vector3 GamePlayScene::EvaluateLevelRoutePoint(float normalizedTime) const {
	if (levelRoutePoints_.empty()) {
		return { 0.0f, 0.0f, 0.0f };
	}
	if (levelRoutePoints_.size() == 1) {
		return levelRoutePoints_.front();
	}

	const float clampedTime = normalizedTime - std::floor(normalizedTime);
	const float scaledTime = clampedTime * static_cast<float>(levelRoutePoints_.size());
	const std::size_t currentIndex = static_cast<std::size_t>(std::floor(scaledTime)) % levelRoutePoints_.size();
	const std::size_t nextIndex = (currentIndex + 1) % levelRoutePoints_.size();
	const float localTime = scaledTime - std::floor(scaledTime);
	return LerpVector3(levelRoutePoints_[currentIndex], levelRoutePoints_[nextIndex], localTime);
}

void GamePlayScene::DrawLevelDebugGizmos() const {
	if (!showLevelGizmos_ || !levelData_) {
		return;
	}

	const Vector4 meshColliderColor = { 0.1f, 0.75f, 1.0f, 1.0f };
	const Vector4 spawnColor = { 0.1f, 1.0f, 0.35f, 1.0f };
	const Vector4 triggerColor = { 1.0f, 0.55f, 0.05f, 1.0f };
	const Vector4 gimmickColor = { 0.85f, 0.35f, 1.0f, 1.0f };
	const Vector4 disabledColor = { 1.0f, 0.1f, 0.1f, 1.0f };
	const Vector4 routeColor = { 0.15f, 0.9f, 1.0f, 1.0f };
	const Vector4 routeActorColor = { 1.0f, 0.9f, 0.15f, 1.0f };
	const Vector4 cameraColor = { 1.0f, 0.35f, 0.9f, 1.0f };
	const Vector4 lightColor = { 1.0f, 0.95f, 0.2f, 1.0f };

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	for (const level::ObjectData& objectData : levelData_->objects) {
		const Vector3 position = objectData.transform.translation;
		if (objectData.disabled) {
			DrawLevelBox(position, { 1.2f, 1.2f, 1.2f }, disabledColor);
			lineDrawer->DrawLine(
				AddVector3(position, { -0.8f, -0.8f, -0.8f }),
				AddVector3(position, { 0.8f, 0.8f, 0.8f }),
				disabledColor);
			lineDrawer->DrawLine(
				AddVector3(position, { -0.8f, 0.8f, -0.8f }),
				AddVector3(position, { 0.8f, -0.8f, 0.8f }),
				disabledColor);
			continue;
		}

		if (objectData.hasCollider) {
			DrawLevelBox(AddVector3(position, objectData.collider.center), objectData.collider.size, meshColliderColor);
		}
		if (objectData.type == kSpawnPointTypeName) {
			lineDrawer->DrawWireSphere(position, 0.5f, spawnColor, 16);
			lineDrawer->DrawLine(position, AddVector3(position, { 0.0f, 1.6f, 0.0f }), spawnColor);
		} else if (objectData.type == kEventTriggerTypeName) {
			const Vector3 size = objectData.hasCollider ? objectData.collider.size : Vector3{ 2.0f, 2.0f, 2.0f };
			const Vector3 center = objectData.hasCollider ? AddVector3(position, objectData.collider.center) : position;
			DrawLevelBox(center, size, triggerColor);
		} else if (objectData.type == kGimmickTypeName) {
			const Vector3 size = objectData.hasCollider ? objectData.collider.size : Vector3{ 1.0f, 2.5f, 0.3f };
			const Vector3 center = objectData.hasCollider ? AddVector3(position, objectData.collider.center) : position;
			DrawLevelBox(center, size, gimmickColor);
		} else if (objectData.type == kPatrolPointTypeName) {
			lineDrawer->DrawWireSphere(position, 0.25f, routeColor, 12);
		} else if (objectData.type == kCameraTypeName) {
			DrawLevelCameraGizmo(position, cameraColor);
		} else if (objectData.type == kLightTypeName) {
			lineDrawer->DrawWireSphere(position, 0.35f, lightColor, 12);
			lineDrawer->DrawLine(position, AddVector3(position, { 0.0f, -1.0f, 0.0f }), lightColor);
		}
	}

	if (levelRoutePoints_.size() >= 2) {
		for (std::size_t i = 0; i < levelRoutePoints_.size(); ++i) {
			const Vector3& current = levelRoutePoints_[i];
			const Vector3& next = levelRoutePoints_[(i + 1) % levelRoutePoints_.size()];
			lineDrawer->DrawLine(current, next, routeColor);
		}

		const Vector3 actorPosition = EvaluateLevelRoutePoint(levelRouteTimer_ * 0.12f);
		lineDrawer->DrawWireSphere(actorPosition, 0.45f, routeActorColor, 16);
		lineDrawer->DrawLine(actorPosition, AddVector3(actorPosition, { 0.0f, 1.0f, 0.0f }), routeActorColor);
	}
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

	auto DrawLevelObjects = [&]() {
		if (!showLevelObjects_) {
			return;
		}
		for (const LevelObjectRuntime& levelObject : levelObjects_) {
			if (levelObject.visible && levelObject.object) {
				levelObject.object->Draw();
			}
		}
		};
	auto DrawLevelObjectShadows = [&]() {
		if (!showLevelObjects_) {
			return;
		}
		for (const LevelObjectRuntime& levelObject : levelObjects_) {
			if (levelObject.visible && levelObject.object) {
				levelObject.object->DrawShadow();
			}
		}
		};

	objCommon->SetShadowStrength(directionalShadowsEnabled_ ? directionalShadowStrength_ : 0.0f);
	if (directionalShadowsEnabled_) {
		Vector3 shadowFocus = levelGameplay_.HasPlayer()
			? levelGameplay_.GetPlayerPosition()
			: Vector3{ 0.0f, 0.0f, 0.0f };
		shadowFocus.y += 4.0f;
		objCommon->UpdateDirectionalShadow(lightDirection_, shadowFocus);
		if (objCommon->BeginShadowPass()) {
			if (showTerrain_ && terrainObj_) terrainObj_->DrawShadow();
			if (showSphere_ && sphereObj_) sphereObj_->DrawShadow();
			if (showPlane_ && object3d_) object3d_->DrawShadow();
			if (showAnimModel_ && animObj_) animObj_->DrawShadow();
			if (showMultiMeshMaterialSample_ && multiMeshMaterialObj_) multiMeshMaterialObj_->DrawShadow();
#ifdef USE_IMGUI
			if (showBusterSword_ && busterSwordAttachment_.IsResolved() && busterSwordObj_) busterSwordObj_->DrawShadow();
#endif
			DrawLevelObjectShadows();
			objCommon->EndShadowPass();
		}
	}
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

	DrawLevelDebugGizmos();
	DrawLevelCollisionGizmos();

	if (modelPriority_ == 0) {
		objCommon->BeginObjectPass();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		if (showMultiMeshMaterialSample_ && multiMeshMaterialObj_) multiMeshMaterialObj_->Draw();
#ifdef USE_IMGUI
		if (showBusterSword_ && busterSwordAttachment_.IsResolved() && busterSwordObj_) busterSwordObj_->Draw();
#endif
		DrawLevelObjects();
		objCommon->EndObjectPass();
		if (skyboxEnabled_ && skybox_) skybox_->Draw();

		if (showSkeleton_ && animObj_ && animObj_->GetSkeleton()) {
			// 【修正】モデルルート行列の二重掛けを防ぐため、GetWorldMatrix() ではなくルートを含まない GetObjectWorldMatrix() を渡します
			skeletonDebugger_->Draw(*animObj_->GetSkeleton(), animObj_->GetObjectWorldMatrix(), LineDrawer::GetInstance(), camera_.get());
		}
#ifdef USE_IMGUI
		DrawBusterSwordAttachmentGizmo();
#endif
		// その直後に呼ばれる
		LineDrawer::GetInstance()->Draw(camera_->GetViewProjectionMatrix());
	}
	else {
		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();

		objCommon->BeginObjectPass();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		if (showMultiMeshMaterialSample_ && multiMeshMaterialObj_) multiMeshMaterialObj_->Draw();
#ifdef USE_IMGUI
		if (showBusterSword_ && busterSwordAttachment_.IsResolved() && busterSwordObj_) busterSwordObj_->Draw();
#endif
		DrawLevelObjects();
		objCommon->EndObjectPass();
		if (skyboxEnabled_ && skybox_) skybox_->Draw();

		if (showSkeleton_ && animObj_ && animObj_->GetSkeleton()) {
			// 【修正】モデルルート行列の二重掛けを防ぐため、GetWorldMatrix() ではなくルートを含まない GetObjectWorldMatrix() を渡します
			skeletonDebugger_->Draw(*animObj_->GetSkeleton(), animObj_->GetObjectWorldMatrix(), LineDrawer::GetInstance(), camera_.get());
		}
#ifdef USE_IMGUI
		DrawBusterSwordAttachmentGizmo();
#endif
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

	if (farmHudInitialized_ && showFarmHud_) {
		spriteCommon->PreDraw();
		farmHud_.Draw();
	}
}

FarmHUDViewData GamePlayScene::BuildFarmHUDViewData() const {
	FarmHUDViewData viewData;
	viewData.day = farmDateSystem_.GetDay();
	viewData.money = 300;
	viewData.rank = 1;
	viewData.timeScale = farmDateSystem_.GetTimeScale();
	viewData.currentToolName = farmToolSystem_.GetCurrentToolName();
	viewData.selectedTileInfo = BuildSelectedTileInfo(farmGrid_);
	return viewData;
}

void GamePlayScene::HandleFarmDateDebugInput() {
	Input* input = framework_->GetInput();
	if (!input) {
		return;
	}
	if (input->PushKey(DIK_LCONTROL) || input->PushKey(DIK_RCONTROL)) {
		return;
	}

	if (input->TriggerKey(InputKey::T)) {
		farmDateSystem_.CycleTimeScale();
	}
	if (input->TriggerKey(InputKey::Y)) {
		farmDateSystem_.AdvanceOneDay();
	}
}

void GamePlayScene::HandleFarmToolDebugInput() {
	Input* input = framework_->GetInput();
	if (!input) {
		return;
	}
	if (viewportHovered_ && input->PushMouseButton(InputMouseButton::Right)) {
		return;
	}

	if (input->TriggerKey(InputKey::E)) {
		farmToolSystem_.SelectNextTool();
	}
	if (input->TriggerKey(InputKey::Q)) {
		farmToolSystem_.SelectPreviousTool();
	}
}

void GamePlayScene::HandleFarmToolActionInput() {
	if (!framework_ || !viewportHovered_ || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}

	Input* input = framework_->GetInput();
	if (!input) {
		return;
	}

	if (input->TriggerKey(InputKey::Enter)) {
		farmToolActionSystem_.ApplyTool(farmGrid_, farmToolSystem_.GetCurrentTool());
	}
}

bool GamePlayScene::HandleFarmGridSelectionInput() {
	if (!framework_ || !viewportHovered_ || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return false;
	}

	Input* input = framework_->GetInput();
	if (!input) {
		return false;
	}

	const bool isArrowPressed =
		input->PushKey(InputKey::ArrowUp) ||
		input->PushKey(InputKey::ArrowDown) ||
		input->PushKey(InputKey::ArrowLeft) ||
		input->PushKey(InputKey::ArrowRight);

	if (input->TriggerKey(InputKey::ArrowUp)) {
		farmGrid_.MoveSelection(0, -1);
		return true;
	}
	if (input->TriggerKey(InputKey::ArrowDown)) {
		farmGrid_.MoveSelection(0, 1);
		return true;
	}
	if (input->TriggerKey(InputKey::ArrowLeft)) {
		farmGrid_.MoveSelection(-1, 0);
		return true;
	}
	if (input->TriggerKey(InputKey::ArrowRight)) {
		farmGrid_.MoveSelection(1, 0);
		return true;
	}
	if (input->TriggerKey(InputKey::PageUp)) {
		farmToolActionSystem_.RaiseSelectedTile(farmGrid_);
		return true;
	}
	if (input->TriggerKey(InputKey::PageDown)) {
		farmToolActionSystem_.LowerSelectedTile(farmGrid_);
		return true;
	}

	return isArrowPressed;
}

void GamePlayScene::InitializeFarmHUD() {
	farmHudInitialized_ = farmHud_.Initialize(framework_->GetSpriteCommon());
	if (!farmHudInitialized_) {
		AddLog("FarmHUD initialization failed.");
		return;
	}

	farmHud_.SetViewData(BuildFarmHUDViewData());
}

void GamePlayScene::InitializeSkyboxIfNeeded() {
	if (skybox_) {
		return;
	}

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(framework_->GetDxCommon(), kRostockSkyboxPath);
}

#ifdef USE_IMGUI
void GamePlayScene::DrawSceneDebugWindow() {
	if (ImGui::Begin("Scene Debug")) {
		ImGui::TextUnformatted("Rendering");
		ImGui::Checkbox("Skybox", &skyboxEnabled_);
		ImGui::Checkbox("Skybox Environment Map", &skyboxEnvironmentEnabled_);
		ImGui::Text("Skybox Loaded: %s", skybox_ ? "Yes" : "No");
		ImGui::Separator();
		ImGui::Checkbox("Terrain", &showTerrain_);
		ImGui::Checkbox("Sphere", &showSphere_);
		ImGui::Checkbox("Plane", &showPlane_);
		ImGui::Checkbox("Level Objects", &showLevelObjects_);
		ImGui::Checkbox("Level Gizmos", &showLevelGizmos_);
		ImGui::Checkbox("Collision Gizmos", &showLevelCollisionGizmos_);
		ImGui::Checkbox("Directional Shadows", &directionalShadowsEnabled_);
		ImGui::SliderFloat("Shadow Strength", &directionalShadowStrength_, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Sun Direction", &lightDirection_.x, 0.01f, -1.0f, 1.0f, "%.2f");
		bool usePlayerCamera = usePlayerCamera_;
		if (ImGui::Checkbox("Use Player Camera (Third Person)", &usePlayerCamera)) {
			SetUsePlayerCamera(usePlayerCamera);
		}
		ImGui::TextUnformatted("F1: Toggle player/debug camera");
		ImGui::TextUnformatted("Hold R: Rewind  |  Shift+R: Forward Scrub");
		ImGui::TextUnformatted("Debug: hold right mouse + WASD, Q/E, wheel");
		if (ImGui::Button("Reload Level (F5 / Ctrl+R)")) {
			LoadSceneLevel();
		}
		ImGui::Text("Timeline: %zu / %zu snapshots  cursor=%zu",
			timeline_.GetSnapshotCount(), timeline_.GetCapacity(), timeline_.GetCursor());
		ImGui::BeginDisabled(!timeline_.CanStepBackward());
		if (ImGui::Button("Step Rewind")) {
			timelineStepBackwardRequested_ = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!timeline_.CanStepForward());
		if (ImGui::Button("Step Forward")) {
			timelineStepForwardRequested_ = true;
		}
		ImGui::EndDisabled();
		ImGui::Text("Level Object3d: %zu", levelObjects_.size());
		ImGui::Text("Collected: %zu / %zu", levelGameplay_.GetCollectedCount(), levelGameplay_.GetCollectibleCount());
		ImGui::Text("Patrol Points: %zu", levelRoutePoints_.size());
		ImGui::Checkbox("Particles", &showParticles_);
	}
	ImGui::End();
}
#endif

void GamePlayScene::UpdateSceneDeltaTime() {
	const FrameClock* frameClock = framework_ ? framework_->GetFrameClock() : nullptr;
	sceneDeltaTime_ = frameClock ? frameClock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds;
}

void GamePlayScene::HandleFarmHistoryInput() {
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (!input || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}
	const bool controlHeld = input->PushKey(DIK_LCONTROL) || input->PushKey(DIK_RCONTROL);
	if (!controlHeld) {
		return;
	}
	const bool shiftHeld = input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT);
	if (input->TriggerKey(InputKey::Z) && shiftHeld) {
		if (farmToolActionSystem_.Redo()) AddLog("Farm Redo");
	} else if (input->TriggerKey(InputKey::Z)) {
		if (farmToolActionSystem_.Undo()) AddLog("Farm Undo");
	} else if (input->TriggerKey(InputKey::Y)) {
		if (farmToolActionSystem_.Redo()) AddLog("Farm Redo");
	}
}

void GamePlayScene::ResetCamera() {
	debugCameraPos_ = { 0.0f, 5.0f, -15.0f };
	debugCameraRot_ = { 0.3f, 0.0f, 0.0f };
	if (usePlayerCamera_ && levelGameplay_.HasPlayer()) {
		UpdatePlayerCamera();
	} else if (hasLevelCamera_) {
		cameraPos_ = levelCameraPos_;
		cameraRot_ = levelCameraRot_;
	} else {
		cameraPos_ = debugCameraPos_;
		cameraRot_ = debugCameraRot_;
	}
}

void GamePlayScene::SetUsePlayerCamera(bool usePlayerCamera) {
	if (usePlayerCamera == usePlayerCamera_) {
		return;
	}

	if (usePlayerCamera) {
		if (!levelGameplay_.HasPlayer()) {
			AddLog("Player camera is not available. PLAYER role is required.");
			return;
		}
		debugCameraPos_ = cameraPos_;
		debugCameraRot_ = cameraRot_;
		usePlayerCamera_ = true;
		UpdatePlayerCamera();
		AddLog("Camera mode: player third-person camera");
	} else {
		cameraPos_ = debugCameraPos_;
		cameraRot_ = debugCameraRot_;
		usePlayerCamera_ = false;
		AddLog("Camera mode: debug camera");
	}
	ClampCameraPitch();
}

void GamePlayScene::ToggleCameraMode() {
	SetUsePlayerCamera(!usePlayerCamera_);
}

void GamePlayScene::UpdatePlayerCamera() {
	if (!usePlayerCamera_ || !levelGameplay_.HasPlayer()) {
		return;
	}

	const Vector3 target = levelGameplay_.GetPlayerPosition() + kPlayerCameraLookOffset;
	cameraPos_ = target + kPlayerCameraOffset;
	const Vector3 toTarget = target - cameraPos_;
	const float distance = MatrixMath::Length(toTarget);
	if (distance <= 0.0001f) {
		return;
	}

	const float normalizedY = std::clamp(toTarget.y / distance, -1.0f, 1.0f);
	cameraRot_.x = -std::asin(normalizedY);
	cameraRot_.y = std::atan2(toTarget.x, toTarget.z);
	cameraRot_.z = 0.0f;
}

void GamePlayScene::ClampCameraPitch() {
	cameraRot_.x = std::clamp(cameraRot_.x, -1.5f, 1.5f);
}

void GamePlayScene::HandleCameraInput(float deltaTime, bool suppressArrowKeys) {
	if (usePlayerCamera_ || !framework_ || !viewportHovered_ || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return;
	}

	Input* input = framework_->GetInput();
	if (!input) {
		return;
	}
	(void)suppressArrowKeys;

	Vector3 right{};
	Vector3 forward{};
	BuildCameraGroundMoveAxes(cameraRot_.y, right, forward);

	Vector3 viewRight{};
	Vector3 viewUp{};
	Vector3 viewForward{};
	BuildCameraViewAxes(cameraRot_, viewRight, viewUp, viewForward);

	const bool isFastMove =
		input->PushKey(InputKey::LeftShift) ||
		input->PushKey(InputKey::RightShift);
	const float speedMultiplier = isFastMove ? kEditorCameraFastMultiplier : 1.0f;

	const bool isRightMouseDown = input->PushMouseButton(InputMouseButton::Right);
	const bool isMiddleMouseDown = input->PushMouseButton(InputMouseButton::Middle);
	const Vector2 mouseDelta = input->GetMouseDelta();

	if (isRightMouseDown) {
		cameraRot_.y += mouseDelta.x * kMouseLookSensitivity;
		cameraRot_.x += mouseDelta.y * kMouseLookSensitivity;

		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (input->PushKey(InputKey::W)) { move += forward; }
		if (input->PushKey(InputKey::S)) { move -= forward; }
		if (input->PushKey(InputKey::D)) { move += right; }
		if (input->PushKey(InputKey::A)) { move -= right; }
		if (input->PushKey(InputKey::E)) { move.y += 1.0f; }
		if (input->PushKey(InputKey::Q)) { move.y -= 1.0f; }

		const float moveLength = MatrixMath::Length(move);
		if (moveLength > 0.0001f) {
			const float moveStep = (std::max)(cameraMoveSpeed_, 0.0f) * speedMultiplier * deltaTime;
			cameraPos_ += (move / moveLength) * moveStep;
		}
	}

	if (isMiddleMouseDown) {
		const float panScale = kMousePanSensitivity * speedMultiplier;
		cameraPos_ += viewRight * (mouseDelta.x * panScale);
		cameraPos_ -= viewUp * (mouseDelta.y * panScale);
	}

	const float wheelDelta = input->GetMouseWheelDelta();
	if (std::abs(wheelDelta) > 0.0f) {
		const float wheelStep = kMouseWheelZoomSpeed * speedMultiplier * deltaTime * wheelDelta;
		cameraPos_ += viewForward * wheelStep;
	}
	ClampCameraPitch();
	debugCameraPos_ = cameraPos_;
	debugCameraRot_ = cameraRot_;
}

void GamePlayScene::HandleKeyboardMovement() {
	if (selectedTarget_ == 0 || viewportHovered_ || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) return;
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
	if (ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
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
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (input && hasBrushPosition && viewportHovered_ &&
		input->PushMouseButton(InputMouseButton::Left) &&
		!ImGuiManager::GetInstance()->WantsCaptureMouse()) {
		const bool isShiftPressed = input->PushKey(InputKey::LeftShift) || input->PushKey(InputKey::RightShift);
		interactionBrushOperation_ = isShiftPressed
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

void GamePlayScene::Finalize()
{
	if (framework_ && framework_->GetAudio()) {
		framework_->GetAudio()->SetGlobalTemporalState(
			AudioPlaybackDirection::Forward,
			1.0f,
			0.0f);
	}
}

#ifdef USE_IMGUI
CG4EvaluationViewData GamePlayScene::BuildCG4EvaluationViewData() const {
	CG4EvaluationViewData viewData;
	ModelManager* modelManager = framework_ ? framework_->GetModelManager() : nullptr;
	viewData.hasAnimatedObject = animObj_ != nullptr;
	viewData.showAnimatedModel = showAnimModel_;
	viewData.showMultiMeshMaterialSample = showMultiMeshMaterialSample_;
	viewData.showWeapon = showBusterSword_;
	viewData.showSkeleton = showSkeleton_;
	viewData.showParticles = showParticles_;
	viewData.showLegacyTools = showLegacySceneDebugWindows_;
	viewData.animationModelIndex = currentAnimModelIdx_;
	viewData.gpuParticleMode = static_cast<int32_t>(gpuParticleDebugMode_);
	viewData.gpuParticleAvailable = framework_ && framework_->GetParticleManager();
	const Model* multiMeshMaterialModel = multiMeshMaterialObj_ ? multiMeshMaterialObj_->GetModel() : nullptr;
	viewData.multiMeshMaterialSampleReady =
		multiMeshMaterialModel &&
		multiMeshMaterialModel->GetMeshCount() > 1 &&
		multiMeshMaterialModel->GetMaterialCount() > 1;
	viewData.weaponAttachmentReady =
		busterSwordObj_ &&
		busterSwordObj_->GetModel() &&
		busterSwordAttachment_.CanResolveJoint();
	viewData.weaponAttachmentActive = busterSwordAttachment_.IsResolved();
	viewData.showWeaponGizmo = showBusterSwordGizmo_;
	viewData.weaponLocalTransform = busterSwordAttachment_.GetLocalTransform();
	viewData.weaponSocketScale = busterSwordAttachment_.GetResolvedParentScale();
	const Model* busterSwordModel = busterSwordObj_ ? busterSwordObj_->GetModel() : nullptr;
	if (busterSwordModel) {
		viewData.weaponVertexCount = busterSwordModel->GetVertexCount();
		viewData.weaponIndexCount = busterSwordModel->GetIndexCount();
		viewData.weaponMeshCount = busterSwordModel->GetMeshCount();
		viewData.weaponMaterialCount = busterSwordModel->GetMaterialCount();
	}

	if (skeletonDebugger_) {
		viewData.selectedJointIndex = skeletonDebugger_->GetSelectedJointIndex();
		viewData.showLocalAxes = skeletonDebugger_->GetShowLocalAxes();
	}

	if (animObj_) {
		viewData.animationPlaying = animObj_->GetIsAnimationPlaying();
		viewData.animationTime = animObj_->GetAnimationTime();
		viewData.animationDuration = animObj_->GetAnimation().duration;
		viewData.animationSpeed = animObj_->GetAnimationSpeed();
		const std::optional<Skeleton>& skeleton = animObj_->GetSkeleton();
		if (skeleton.has_value()) {
			viewData.skeleton = &skeleton.value();
			viewData.jointCount = static_cast<uint32_t>(skeleton->joints.size());
		}

		const Model* model = animObj_->GetModel();
		if (model) {
			if (modelManager &&
				currentAnimModelIdx_ >= 0 &&
				currentAnimModelIdx_ < static_cast<int32_t>(kAnimationModels.size())) {
				const AnimationModelDefinition& definition = kAnimationModels[static_cast<size_t>(currentAnimModelIdx_)];
				viewData.animationSkinningIsolated =
					model == modelManager->GetModel(definition.evaluationCacheKey);
			}
			viewData.hasSkinCluster = model->HasSkinCluster();
			viewData.computeSkinningEnabled = model->UseComputeSkinning();
			viewData.vertexCount = model->GetVertexCount();
			viewData.indexCount = model->GetIndexCount();
			viewData.meshCount = model->GetMeshCount();
			viewData.materialCount = model->GetMaterialCount();
		}
	}
	if (showMultiMeshMaterialSample_ && multiMeshMaterialModel) {
		viewData.vertexCount = multiMeshMaterialModel->GetVertexCount();
		viewData.indexCount = multiMeshMaterialModel->GetIndexCount();
		viewData.meshCount = multiMeshMaterialModel->GetMeshCount();
		viewData.materialCount = multiMeshMaterialModel->GetMaterialCount();
		viewData.jointCount = 0;
	} else if (showBusterSword_ && busterSwordModel) {
		viewData.vertexCount = busterSwordModel->GetVertexCount();
		viewData.indexCount = busterSwordModel->GetIndexCount();
		viewData.meshCount = busterSwordModel->GetMeshCount();
		viewData.materialCount = busterSwordModel->GetMaterialCount();
	}
	return viewData;
}

void GamePlayScene::ApplyCG4EvaluationActions(const CG4EvaluationActions& actions) {
	if (actions.showLegacyTools.has_value()) {
		showLegacySceneDebugWindows_ = *actions.showLegacyTools;
	}

	auto configureEvaluationView = [this]() {
		showTerrain_ = false;
		showSphere_ = false;
		showPlane_ = false;
		showSprite_ = false;
		showLevelObjects_ = false;
		showLevelGizmos_ = false;
		showLevelCollisionGizmos_ = false;
		showFarmHud_ = false;
		showMultiMeshMaterialSample_ = false;
		showBusterSword_ = false;
		SetUsePlayerCamera(false);
		// Frame the roughly two-meter sample character tightly enough that joint
		// colors and the selected-joint axes remain readable in the split viewport.
		cameraPos_ = { 0.0f, 0.25f, -7.0f };
		cameraRot_ = { 0.18f, 0.0f, 0.0f };
		debugCameraPos_ = cameraPos_;
		debugCameraRot_ = cameraRot_;
	};

	if (actions.preset.has_value()) {
		switch (*actions.preset) {
		case CG4EvaluationPreset::Gameplay:
			showAnimModel_ = false;
			showMultiMeshMaterialSample_ = false;
			showBusterSword_ = false;
			showSkeleton_ = false;
			showTerrain_ = false;
			showSphere_ = false;
			showPlane_ = false;
			showSprite_ = false;
			showLevelObjects_ = true;
			showLevelGizmos_ = false;
			showLevelCollisionGizmos_ = false;
			showParticles_ = true;
			showFarmHud_ = true;
			SetGPUParticleDebugMode(GPUParticleDebugMode::Off);
			SetUsePlayerCamera(true);
			break;
		case CG4EvaluationPreset::Skinning:
			configureEvaluationView();
			currentAnimModelIdx_ = 2;
			ChangeAnimationModel(2);
			showAnimModel_ = true;
			showSkeleton_ = false;
			showParticles_ = false;
			SetGPUParticleDebugMode(GPUParticleDebugMode::Off);
			break;
		case CG4EvaluationPreset::Skeleton:
			configureEvaluationView();
			currentAnimModelIdx_ = 2;
			ChangeAnimationModel(2);
			showAnimModel_ = true;
			showSkeleton_ = true;
			showParticles_ = false;
			SetGPUParticleDebugMode(GPUParticleDebugMode::Off);
			break;
		case CG4EvaluationPreset::WeaponAttachment:
			configureEvaluationView();
			currentAnimModelIdx_ = 2;
			ChangeAnimationModel(2);
			showAnimModel_ = true;
			showSkeleton_ = false;
			showBusterSword_ = true;
			showParticles_ = false;
			SetGPUParticleDebugMode(GPUParticleDebugMode::Off);
			break;
		case CG4EvaluationPreset::MultiMeshMaterial:
			configureEvaluationView();
			showAnimModel_ = false;
			showSkeleton_ = false;
			showMultiMeshMaterialSample_ = true;
			showParticles_ = false;
			SetGPUParticleDebugMode(GPUParticleDebugMode::Off);
			break;
		case CG4EvaluationPreset::GpuParticle:
			configureEvaluationView();
			showAnimModel_ = false;
			showSkeleton_ = false;
			showParticles_ = true;
			SetGPUParticleDebugMode(GPUParticleDebugMode::Agriculture);
			EmitAgricultureParticle(AgricultureParticleType::HarvestSparkle);
			break;
		}
	}

	if (actions.animationModelIndex.has_value()) {
		const int32_t lastModelIndex = static_cast<int32_t>(kAnimationModels.size()) - 1;
		const int32_t modelIndex = std::clamp(*actions.animationModelIndex, 0, lastModelIndex);
		if (!animObj_ || modelIndex != currentAnimModelIdx_) {
			currentAnimModelIdx_ = modelIndex;
			ChangeAnimationModel(modelIndex);
		}
	}
	if (actions.showAnimatedModel.has_value()) {
		showAnimModel_ = *actions.showAnimatedModel;
	}
	if (actions.showSkeleton.has_value()) {
		showSkeleton_ = *actions.showSkeleton;
	}
	if (actions.showParticles.has_value()) {
		showParticles_ = *actions.showParticles;
	}
	if (actions.showWeapon.has_value()) {
		showBusterSword_ = *actions.showWeapon;
	}
	if (actions.showWeaponGizmo.has_value()) {
		showBusterSwordGizmo_ = *actions.showWeaponGizmo;
	}
	if (actions.resetWeaponTransform) {
		if (!busterSwordAttachment_.SetLocalTransform(kDefaultBusterSwordTransform)) {
			AddLog("BusterSword attachment: default local Transform was rejected.");
		}
	} else if (actions.weaponLocalTransform.has_value()) {
		if (!busterSwordAttachment_.SetLocalTransform(*actions.weaponLocalTransform)) {
			AddLog("BusterSword attachment: invalid local Transform was rejected.");
		}
	}

	if (animObj_) {
		if (actions.computeSkinningEnabled.has_value()) {
			if (Model* model = animObj_->GetModel(); model && model->HasSkinCluster()) {
				model->SetUseComputeSkinning(*actions.computeSkinningEnabled);
			}
		}
		if (actions.animationPlaying.has_value()) {
			animObj_->GetIsAnimationPlaying() = *actions.animationPlaying;
		}
		if (actions.animationSpeed.has_value() && std::isfinite(*actions.animationSpeed)) {
			animObj_->GetAnimationSpeed() = std::clamp(*actions.animationSpeed, -2.0f, 2.0f);
		}
		if (actions.resetAnimation) {
			animObj_->GetAnimationTime() = 0.0f;
		}
		if (actions.animationTime.has_value() && std::isfinite(*actions.animationTime)) {
			const float duration = (std::max)(animObj_->GetAnimation().duration, 0.0f);
			animObj_->GetAnimationTime() = std::clamp(*actions.animationTime, 0.0f, duration);
		}
	}

	if (skeletonDebugger_) {
		if (actions.showLocalAxes.has_value()) {
			skeletonDebugger_->SetShowLocalAxes(*actions.showLocalAxes);
		}
		const Skeleton* skeleton = nullptr;
		if (animObj_ && animObj_->GetSkeleton().has_value()) {
			skeleton = &animObj_->GetSkeleton().value();
		}
		if (skeleton && !skeleton->joints.empty()) {
			int32_t selectedJoint = actions.selectedJointIndex.value_or(skeletonDebugger_->GetSelectedJointIndex());
			if (actions.preset == CG4EvaluationPreset::Skeleton ||
				actions.preset == CG4EvaluationPreset::WeaponAttachment) {
				const auto rightHand = skeleton->jointMap.find("mixamorig:RightHand");
				if (rightHand != skeleton->jointMap.end()) {
					selectedJoint = rightHand->second;
				}
			}
			selectedJoint = std::clamp(selectedJoint, 0, static_cast<int32_t>(skeleton->joints.size()) - 1);
			skeletonDebugger_->SetSelectedJointIndex(selectedJoint);
		} else {
			skeletonDebugger_->SetSelectedJointIndex(-1);
		}
	}

	if (actions.emitGpuParticleSample) {
		showParticles_ = true;
		SetGPUParticleDebugMode(GPUParticleDebugMode::Agriculture);
		EmitAgricultureParticle(AgricultureParticleType::HarvestSparkle);
	}
}
#endif

/**
 * ChangeAnimationModel: インデックスに応じて再生するアニメーションモデルを動的に切り替える
 * 各モデル固有のサイズ（スケール）や初期位置の自動調整もここで行います
 */
void GamePlayScene::ChangeAnimationModel(int index) {
	if (index < 0 || index >= static_cast<int>(kAnimationModels.size())) return;

	const AnimationModelDefinition& definition = kAnimationModels[static_cast<size_t>(index)];
	const std::string resourcePath = std::string(definition.directory) + "/" + definition.fileName;
#ifdef USE_IMGUI
	const std::string modelKey = definition.evaluationCacheKey;
#else
	const std::string& modelKey = resourcePath;
#endif
	Model* animModel = framework_->GetModelManager()->GetModel(modelKey);
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
		if (skybox_ && skyboxEnvironmentEnabled_) {
			animObj_->SetEnvironmentMap(skybox_->GetTextureHandle());
		}
		
		// アニメーションデータをアセットフォルダから読み込んでオブジェクトにセット
		Animation anim = animModel->LoadAnimation(
			std::string("Resources/") + definition.directory,
			definition.fileName);
		animObj_->SetAnimation(anim);
		
		// 再生制御パラメータの初期化
		animObj_->GetAnimationTime() = 0.0f;
		animObj_->GetIsAnimationPlaying() = true;
#ifdef USE_IMGUI
		BindBusterSwordAttachment();
#endif
	}
}

#ifdef USE_IMGUI
void GamePlayScene::BindBusterSwordAttachment() {
	if (!busterSwordObj_ || !busterSwordObj_->GetModel() || !animObj_) {
		busterSwordAttachment_.Clear();
		return;
	}

	JointAttachmentSystem::BindingSettings settings;
	settings.jointName = "mixamorig:RightHand";
	settings.localTransform = kDefaultBusterSwordTransform;
	if (!busterSwordAttachment_.Bind(busterSwordObj_.get(), animObj_.get(), settings)) {
		AddLog("BusterSword attachment: RightHand Joint is unavailable for the selected model.");
	}
}

void GamePlayScene::DrawBusterSwordAttachmentGizmo() const {
	if (!showBusterSword_ || !showBusterSwordGizmo_ || !busterSwordObj_ || !busterSwordAttachment_.IsResolved()) {
		return;
	}
	constexpr Vector3 kGripLocalPosition = { 0.0f, 0.0f, 0.0f };
	constexpr Vector3 kBladeTipLocalPosition = { 0.0f, 219.05f, 0.0f };
	// BusterSword.obj の頂点範囲。Socket調整時だけ描くため、Modelの描画契約には持ち込まない。
	constexpr Vector3 kBoundsMin = { -32.884f, -11.213f, -4.178f };
	constexpr Vector3 kBoundsMax = { 33.097f, 219.050f, 4.019f };
	constexpr float kLocalAxisLength = 12.0f;
	const Matrix4x4& weaponWorldMatrix = busterSwordObj_->GetObjectWorldMatrix();
	const Vector3 gripPosition = MatrixMath::Transform(kGripLocalPosition, weaponWorldMatrix);
	const Vector3 bladeTipPosition = MatrixMath::Transform(kBladeTipLocalPosition, weaponWorldMatrix);
	const Vector3 axisX = MatrixMath::Transform({ kLocalAxisLength, 0.0f, 0.0f }, weaponWorldMatrix);
	const Vector3 axisY = MatrixMath::Transform({ 0.0f, kLocalAxisLength, 0.0f }, weaponWorldMatrix);
	const Vector3 axisZ = MatrixMath::Transform({ 0.0f, 0.0f, kLocalAxisLength }, weaponWorldMatrix);
	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	lineDrawer->DrawLine(gripPosition, axisX, { 1.0f, 0.15f, 0.15f, 1.0f });
	lineDrawer->DrawLine(gripPosition, axisY, { 0.15f, 1.0f, 0.20f, 1.0f });
	lineDrawer->DrawLine(gripPosition, axisZ, { 0.20f, 0.45f, 1.0f, 1.0f });
	LineDrawer::GetInstance()->DrawLine(
		gripPosition, bladeTipPosition, { 1.0f, 0.15f, 0.85f, 1.0f });
	LineDrawer::GetInstance()->DrawWireSphere(
		bladeTipPosition, 0.06f, { 1.0f, 0.15f, 0.85f, 1.0f }, 8);

	const Vector3 localCorners[8] = {
		{ kBoundsMin.x, kBoundsMin.y, kBoundsMin.z },
		{ kBoundsMax.x, kBoundsMin.y, kBoundsMin.z },
		{ kBoundsMax.x, kBoundsMax.y, kBoundsMin.z },
		{ kBoundsMin.x, kBoundsMax.y, kBoundsMin.z },
		{ kBoundsMin.x, kBoundsMin.y, kBoundsMax.z },
		{ kBoundsMax.x, kBoundsMin.y, kBoundsMax.z },
		{ kBoundsMax.x, kBoundsMax.y, kBoundsMax.z },
		{ kBoundsMin.x, kBoundsMax.y, kBoundsMax.z },
	};
	Vector3 corners[8]{};
	for (size_t i = 0; i < std::size(localCorners); ++i) {
		corners[i] = MatrixMath::Transform(localCorners[i], weaponWorldMatrix);
	}
	constexpr size_t kEdges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};
	for (const auto& edge : kEdges) {
		lineDrawer->DrawLine(corners[edge[0]], corners[edge[1]], { 1.0f, 0.25f, 0.75f, 0.75f });
	}
}
#endif
