#include "application/scene/MagnetPrototypeScene.h"
#include "application/scene/GameFlowState.h"
#include "application/scene/SceneManager.h"

#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Skybox.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "io/Input.h"
#include "effect/ComicTextEffect.h"
#include "effect/ParticleManager.h"
#ifdef USE_IMGUI
#include "debug/ParticleEffectEditor.h"
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <system_error>

namespace {

#ifdef MAGNET_STARTUP_STAGE_OBSTACLE
constexpr char kReleaseStageSaveName[] = "stage_Obstacle";
#endif

constexpr Vector4 kPlayerColor = { 1.0f, 0.15f, 0.12f, 1.0f };
constexpr Vector4 kLeftChainColor = { 0.15f, 0.65f, 1.0f, 1.0f };
constexpr Vector4 kRightChainColor = { 0.25f, 0.9f, 0.75f, 1.0f };
constexpr Vector4 kAvailableBallColor = { 1.0f, 0.8f, 0.12f, 1.0f };
constexpr Vector4 kReleasedBallColor = { 1.0f, 0.42f, 0.08f, 1.0f };
constexpr Vector4 kConstraintColor = { 0.82f, 0.86f, 0.92f, 1.0f };
constexpr Vector4 kGridColor = { 0.18f, 0.22f, 0.28f, 1.0f };
constexpr Vector4 kVelocityColor = { 1.0f, 0.55f, 0.08f, 1.0f };
constexpr Vector4 kGoalColor = { 0.18f, 1.0f, 0.30f, 1.0f };
constexpr Vector4 kObstacleColor = { 0.72f, 0.75f, 0.82f, 1.0f };
constexpr Vector4 kChainsawColor = { 1.0f, 0.18f, 0.12f, 1.0f };
constexpr Vector4 kBumperColor = { 0.10f, 0.85f, 1.0f, 1.0f };
constexpr Vector4 kFurnaceColor = { 1.0f, 0.42f, 0.05f, 1.0f };
constexpr Vector4 kAnchorColor = { 0.72f, 0.30f, 1.0f, 1.0f };
constexpr Vector4 kAnchorFieldColor = { 0.62f, 0.22f, 1.0f, 0.25f };
constexpr Vector4 kShutterClosedColor = { 1.0f, 0.82f, 0.12f, 1.0f };
constexpr Vector4 kShutterOpenColor = { 0.30f, 0.95f, 0.50f, 0.85f };
constexpr Vector4 kTransferGateColor = { 0.12f, 0.95f, 0.88f, 1.0f };
constexpr Vector4 kRepulsionFieldColor = { 1.0f, 0.20f, 0.62f, 1.0f };
constexpr Vector4 kRepulsionRangeColor = { 1.0f, 0.30f, 0.68f, 0.38f };
constexpr Vector4 kSelectionColor = { 1.0f, 0.12f, 0.85f, 1.0f };
constexpr const char* kPlayerModelPath = "magnet/player/player.obj";
constexpr const char* kSmallBallModelPath = "magnet/small_ball/SmallBall.obj";
constexpr float kGridSpacing = 1.0f;
constexpr float kAuthoredArenaRadius = 20.0f;
constexpr float kVelocityDisplayScale = 0.22f;
constexpr int kArenaWallSegments = 64;
constexpr int kAnchorFieldSegments = 32;
constexpr int kRepulsionFieldSegments = 24;
constexpr int kRepulsionArrowCount = 8;
constexpr float kArenaWallHeight = 1.4f;
constexpr float kCameraBlend = 0.14f;
constexpr float kSelectionSpherePadding = 0.18f;
constexpr float kSelectionBoxPadding = 0.18f;
constexpr float kMinimapSize = 144.0f;
constexpr float kMinimapBorderSize = 148.0f;
constexpr float kMinimapMargin = 16.0f;
constexpr float kMinimapWorldRadius = 24.0f;
constexpr float kMinimapMarkerSize = 8.0f;
constexpr float kMinimapPlayerSize = 14.0f;
constexpr float kMinimapGuideThickness = 1.0f;
constexpr float kMinimapGuideInset = 10.0f;
constexpr Vector2 kVirtualScreenSize = { 1280.0f, 720.0f };
constexpr float kGoalGuideScreenMargin = 38.0f;
constexpr float kGoalGuideArmLength = 18.0f;
constexpr float kGoalGuideThickness = 4.0f;
constexpr float kGoalGuideHalfAngle = 0.70f;
constexpr int kPauseMenuItemCount = 5;
constexpr Vector4 kUiTextColor = { 0.88f, 0.94f, 0.98f, 1.0f };
constexpr Vector4 kUiAccentColor = { 1.0f, 0.82f, 0.24f, 1.0f };
constexpr float kComicTextMinimumImpactSpeed = 6.0f;
constexpr const char* kMagneticImpactSoundPath =
	"Resources/audio/magnet/kaveHannsya.mp3";
constexpr const char* kMagneticAttachmentSoundPath =
	"Resources/audio/magnet/kuttuku.wav";
constexpr const char* kMagneticGoalSoundPath =
	"Resources/audio/magnet/goal.wav";
constexpr const char* kChainsawLoopSoundPath =
	"Resources/audio/magnet/Chainsaw.wav";
constexpr const char* kChainsawCutSoundPath =
	"Resources/audio/magnet/cuts.wav";
constexpr magnet::ChainsawProximitySoundSystem::Settings kChainsawLoopSoundSettings{
	10.0f,
	2.5f,
	0.32f,
	1.0f,
	7.0f,
};
constexpr magnet::MagneticOneShotSoundSystem::Settings kChainsawCutSoundSettings{
	0.72f,
	1.0f,
};

[[nodiscard]] bool IsFiniteVector3(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool IsRegularFileNoThrow(
	const std::filesystem::path& path) noexcept
{
	std::error_code error;
	return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] bool HasMovementInput(const Vector3& direction) noexcept
{
	return std::fabs(direction.x) > 0.0001f || std::fabs(direction.z) > 0.0001f;
}

[[nodiscard]] Vector3 RotateByQuaternion(
	const Vector3& vector,
	const Quaternion& rotation) noexcept
{
	const Vector3 quaternionVector{ rotation.x, rotation.y, rotation.z };
	const Vector3 twiceCross{
		2.0f * (quaternionVector.y * vector.z - quaternionVector.z * vector.y),
		2.0f * (quaternionVector.z * vector.x - quaternionVector.x * vector.z),
		2.0f * (quaternionVector.x * vector.y - quaternionVector.y * vector.x),
	};
	const Vector3 secondCross{
		quaternionVector.y * twiceCross.z - quaternionVector.z * twiceCross.y,
		quaternionVector.z * twiceCross.x - quaternionVector.x * twiceCross.z,
		quaternionVector.x * twiceCross.y - quaternionVector.y * twiceCross.x,
	};
	return vector + twiceCross * rotation.w + secondCross;
}

[[nodiscard]] Vector4 GetObstacleColor(
	magnet::MagnetObstacleKind kind,
	bool shutterClosed) noexcept
{
	switch (kind) {
	case magnet::MagnetObstacleKind::Chainsaw: return kChainsawColor;
	case magnet::MagnetObstacleKind::PinballBumper: return kBumperColor;
	case magnet::MagnetObstacleKind::Furnace: return kFurnaceColor;
	case magnet::MagnetObstacleKind::MagneticAnchor: return kAnchorColor;
	case magnet::MagnetObstacleKind::TimedShutter:
		return shutterClosed ? kShutterClosedColor : kShutterOpenColor;
	case magnet::MagnetObstacleKind::TransferGate: return kTransferGateColor;
	case magnet::MagnetObstacleKind::RepulsionField: return kRepulsionFieldColor;
	case magnet::MagnetObstacleKind::Solid:
	default:
		return kObstacleColor;
	}
}

[[nodiscard]] Vector4 GetStageBallColor(
	magnet::MagnetChainSystem::StageBallState state) noexcept
{
	switch (state) {
	case magnet::MagnetChainSystem::StageBallState::AttachedLeft:
		return kLeftChainColor;
	case magnet::MagnetChainSystem::StageBallState::AttachedRight:
		return kRightChainColor;
	case magnet::MagnetChainSystem::StageBallState::Released:
		return kReleasedBallColor;
	case magnet::MagnetChainSystem::StageBallState::Available:
	case magnet::MagnetChainSystem::StageBallState::Inactive:
	default:
		return kAvailableBallColor;
	}
}

} // namespace

void MagnetPrototypeScene::Initialize()
{
	framework_ = Framework::GetInstance();
	assert(framework_ && "Framework must exist before MagnetPrototypeScene initialization.");

	camera_ = std::make_unique<Camera>();
	camera_->SetTranslate({ 0.0f, 11.0f, -16.0f });
	camera_->SetRotate({ 0.60f, 0.0f, 0.0f });
	camera_->Update();
	magnetEditorCameraSystem_.Reset();
	skybox_ = std::make_unique<Skybox>();
	skybox_->InitializeGradient(
		framework_->GetDxCommon(),
		{ 0.22f, 0.38f, 0.62f, 1.0f },
		{ 0.70f, 0.30f, 0.34f, 1.0f });
	skybox_->Update(camera_.get());
	constexpr const char* kGroundModelPath = "stage/MagnetGround.obj";
	framework_->GetModelManager()->LoadModel(kGroundModelPath);
	if (Model* groundModel = framework_->GetModelManager()->GetModel(kGroundModelPath)) {
		groundModel->LoadTextures();
		groundVisual_ = std::make_unique<Object3d>();
		groundVisual_->Initialize(framework_->GetObject3dCommon());
		groundVisual_->SetModel(groundModel);
		groundVisual_->SetPosition({ 0.0f, -0.025f, 0.0f });
		groundVisual_->SetEnableLighting(false);
		groundVisual_->SetCullMode(0);
		groundVisual_->Update(camera_.get(), 0.0f);
	}
	constexpr const char* kGlassWallModelPath = "stage/ArenaGlassWall.obj";
	framework_->GetModelManager()->LoadModel(kGlassWallModelPath);
	if (Model* glassModel = framework_->GetModelManager()->GetModel(kGlassWallModelPath)) {
		glassModel->LoadTextures();
		glassWallVisual_ = std::make_unique<Object3d>();
		glassWallVisual_->Initialize(framework_->GetObject3dCommon());
		glassWallVisual_->SetModel(glassModel);
		glassWallVisual_->SetColor({ 0.70f, 0.90f, 1.0f, 0.72f });
		glassWallVisual_->SetEnableLighting(false);
		glassWallVisual_->SetCullMode(0);
		glassWallVisual_->Update(camera_.get(), 0.0f);
	}
	LineDrawer::GetInstance()->Initialize(framework_->GetDxCommon());
	minimapReady_ = InitializeMinimap();
	if (!minimapReady_) {
		Logger::Log("MagnetPrototypeScene: native minimap initialization failed.");
	}
	goalGuidesReady_ = InitializeGoalGuides();
	if (!goalGuidesReady_) {
		Logger::Log("MagnetPrototypeScene: goal guide initialization failed.");
	}
	gameFlowUiReady_ = InitializeGameFlowUi();
	tutorialUiReady_ = !tutorialMode_ || InitializeTutorialUi();
	GameFlowState::GetInstance().EnsureBgm(
		framework_->GetAudio(), GameFlowState::BgmTrack::Gameplay);
	if (!magneticImpactSoundSystem_.Initialize(
		framework_->GetAudio(), kMagneticImpactSoundPath)) {
		Logger::Log(
			"MagnetPrototypeScene: magnetic impact SE could not be loaded; "
			"gameplay will continue without it.");
	}
	if (!magneticAttachmentSoundSystem_.Initialize(
		framework_->GetAudio(), kMagneticAttachmentSoundPath)) {
		Logger::Log(
			"MagnetPrototypeScene: magnetic attachment SE could not be loaded; "
			"gameplay will continue without it.");
	}
	if (!magneticGoalSoundSystem_.Initialize(
		framework_->GetAudio(), kMagneticGoalSoundPath)) {
		Logger::Log(
			"MagnetPrototypeScene: magnetic goal SE could not be loaded; "
			"gameplay will continue without it.");
	}
	if (!chainsawProximitySoundSystem_.Initialize(
		framework_->GetAudio(), kChainsawLoopSoundPath, kChainsawLoopSoundSettings)) {
		Logger::Log(
			"MagnetPrototypeScene: Chainsaw loop SE could not be loaded; "
			"gameplay will continue without it.");
	}
	if (!chainsawCutSoundSystem_.Initialize(
		framework_->GetAudio(), kChainsawCutSoundPath, kChainsawCutSoundSettings)) {
		Logger::Log(
			"MagnetPrototypeScene: Chainsaw cut SE could not be loaded; "
			"gameplay will continue without it.");
	}
	if (!gimmickSoundSystem_.Initialize(framework_->GetAudio())) {
		Logger::Log(
			"MagnetPrototypeScene: one or more gimmick sounds could not be loaded; "
			"gameplay will continue without those sounds.");
	}
	if (!gimmickEffectSystem_.Initialize(framework_->GetParticleManager())) {
		Logger::Log(
			"MagnetPrototypeScene: gimmick particle effects are unavailable; "
			"line effects will continue to work.");
	}

	bool stageReady = false;
	GameFlowState& gameFlowState = GameFlowState::GetInstance();
	const std::string& activeStageSaveName =
		gameFlowState.GetActiveStageSaveName();
	if (!activeStageSaveName.empty()) {
		stageReady = magnetStageSystem_.LoadNamed(activeStageSaveName);
		if (!stageReady) {
			Logger::Log(
				"MagnetPrototypeScene: active stage reload failed; falling back to startup stage: " +
				magnetStageSystem_.GetLastOperationMessage());
		}
	}
	if (!stageReady) {
#ifdef MAGNET_STARTUP_STAGE_OBSTACLE
		stageReady = magnetStageSystem_.LoadNamed(kReleaseStageSaveName);
#else
		stageReady = magnetStageSystem_.Initialize();
#endif
	}
	if (stageReady) {
		gameFlowState.SetActiveStageSaveName(
			magnetStageSystem_.GetStageData().name);
	}
	if (!stageReady) {
		Logger::Log(
			"MagnetPrototypeScene: startup stage initialization failed: " +
			magnetStageSystem_.GetLastOperationMessage());
	}
	if (stageReady && tutorialMode_) {
		while (magnetStageSystem_.GetStageData().ballCount > 0) {
			(void)magnetStageSystem_.RemoveBall(
				magnetStageSystem_.GetStageData().balls[0].id);
		}
		while (magnetStageSystem_.GetStageData().goalCount > 0) {
			(void)magnetStageSystem_.RemoveBoxObject(
				magnet::MagnetStageObjectType::Goal,
				magnetStageSystem_.GetStageData().goals[0].id);
		}
		while (magnetStageSystem_.GetStageData().obstacleCount > 0) {
			(void)magnetStageSystem_.RemoveBoxObject(
				magnet::MagnetStageObjectType::Obstacle,
				magnetStageSystem_.GetStageData().obstacles[0].id);
		}
		(void)magnetStageSystem_.SetPlayerPosition({ 0.0f, 0.75f, 0.0f });
	}
	prototypeReady_ = stageReady &&
		magnetChainSystem_.Initialize(magnetStageSystem_.GetStageData());
	if (prototypeReady_ && tutorialMode_) {
		magnetChainSystem_.SetActiveGoalCount(0);
		magnetChainSystem_.SetActiveObstacleCount(0);
	}
	ballVisualsReady_ = prototypeReady_ && InitializeBallVisuals();
	if (ballVisualsReady_) {
		ballVisualsReady_ = UpdateBallVisuals(0.0f);
	}
	if (!ballVisualsReady_) {
		Logger::Log(
			"MagnetPrototypeScene: Player/SmallBall model initialization failed; using wire fallback.");
	}
	furnaceVisualsReady_ = furnaceVisualSystem_.Initialize(
		framework_->GetObject3dCommon(),
		framework_->GetModelManager(),
		camera_.get());
	if (!furnaceVisualsReady_) {
		Logger::Log(
			"MagnetPrototypeScene: Furnace visual initialization failed; using wire fallback.");
	}
	stageStructureVisualsReady_ = magnetStageStructureVisualSystem_.Initialize(
		framework_->GetObject3dCommon(),
		framework_->GetModelManager(),
		camera_.get());
	if (!stageStructureVisualsReady_) {
		Logger::Log(
			"MagnetPrototypeScene: Goal/Wall visual initialization failed; using wire fallback.");
	}
	magnetGimmickVisualsReady_ = magnetGimmickVisualSystem_.Initialize(
		framework_->GetObject3dCommon(),
		framework_->GetModelManager(),
		camera_.get());
	if (!magnetGimmickVisualsReady_) {
		Logger::Log(
			"MagnetPrototypeScene: Transfer/repulsion/anchor visual initialization failed; "
			"using wire fallback.");
	}
	magneticImpactFeedbackSystem_.Reset();
	comicTextEffects_ = std::make_unique<ComicTextEffectSystem>();
	comicTextEffects_->Initialize(framework_->GetSpriteCommon());
	if (!gimmickComicTextSystem_.Initialize(comicTextEffects_.get())) {
		Logger::Log(
			"MagnetPrototypeScene: gimmick comic text effects are unavailable; "
			"gameplay will continue without captions.");
	}
	ComicTextEffectSystem::LoadPreset("HeavyImpact", heavyImpactPreset_);
	if (!prototypeReady_) {
		Logger::Log("MagnetPrototypeScene: magnet prototype initialization failed.");
		assert(false && "Magnet prototype initialization failed.");
	}
	pendingCommand_ = {};
	resetRequested_ = false;
	showGrid_ = true;
	showVelocity_ = true;
	cameraFollow_ = true;
	editorMode_ = magnet::MagnetEditorMode::Play;
	selectedObjectType_ = magnet::MagnetStageObjectType::None;
	selectedObjectId_ = 0;
	releaseOverviewActive_ = false;
	gameElapsedSeconds_ = 0.0f;
	pauseSelection_ = 0;
	paused_ = false;
	rankingTransitionRequested_ = false;
	rightTriggerWasPressed_ = false;
	menuStickUpWasPressed_ = false;
	menuStickDownWasPressed_ = false;
	menuStickLeftWasPressed_ = false;
	menuStickRightWasPressed_ = false;
	tutorialPhase_ = TutorialPhase::Movement;
	tutorialMovementDistance_ = 0.0f;
	tutorialScoreAtGoalStart_ = 0;
#ifdef USE_IMGUI
	particleEffectEditor_ = std::make_unique<ParticleEffectEditor>();
#endif
	RefreshGameFlowUi();
	UpdateTutorialUi();
}

void MagnetPrototypeScene::Finalize()
{
#ifdef USE_IMGUI
	particleEffectEditor_.reset();
#endif
	if (framework_ && framework_->GetParticleManager()) {
		framework_->GetParticleManager()->ClearAll();
		framework_->GetParticleManager()->ResetGPUParticles();
	}
	gimmickComicTextSystem_.Finalize();
	comicTextEffects_.reset();
	gimmickEffectSystem_.Finalize();
	magnetGimmickVisualSystem_.Finalize();
	magnetGimmickVisualsReady_ = false;
	magnetStageStructureVisualSystem_.Finalize();
	stageStructureVisualsReady_ = false;
	furnaceVisualSystem_.Finalize();
	furnaceVisualsReady_ = false;
	gimmickSoundSystem_.Finalize();
	chainsawCutSoundSystem_.Finalize();
	chainsawProximitySoundSystem_.Finalize();
	magneticGoalSoundSystem_.Finalize();
	magneticAttachmentSoundSystem_.Finalize();
	magneticImpactSoundSystem_.Finalize();
	playerVisual_.reset();
	for (auto& visual : stageBallVisuals_) { visual.reset(); }
	ballVisualsReady_ = false;
	for (auto& sprite : timerDigitSprites_) { sprite.reset(); }
	timerDigitsReady_ = false;
	timeHudObject_.reset();
	scoreHudObject_.reset();
	seVolumeLabelObject_.reset();
	volumeLabelObject_.reset();
	backTitleLabelObject_.reset();
	restartLabelObject_.reset();
	resumeLabelObject_.reset();
	pauseTitleObject_.reset();
	pauseLabelCamera_.reset();
	pauseOverlaySprite_.reset();
	gameFlowUiReady_ = false;
	for (auto& sprite : tutorialKeySprites_) { sprite.reset(); }
	for (auto& sprite : tutorialKeyImageSprites_) { sprite.reset(); }
	tutorialMoveGuideSprite_.reset();
	tutorialAttractGuideSprite_.reset();
	tutorialShootGuideSprite_.reset();
	tutorialGimmickGuideSprite_.reset();
	tutorialSkipSprite_.reset();
	tutorialSkipEnterSprite_.reset();
	tutorialTransitionKeyObject_.reset();
	tutorialObstacleGuidePanel_.reset();
	for (auto& sprite : tutorialObstacleIcons_) { sprite.reset(); }
	tutorialObstacleGuideVisible_ = false;
	tutorialUiReady_ = false;
	for (auto& guide : goalGuideSprites_) {
		for (auto& arm : guide) { arm.reset(); }
	}
	goalGuideCount_ = 0;
	goalGuidesReady_ = false;
	for (auto& marker : minimapMagnetSprites_) { marker.reset(); }
	minimapPlayerSprite_.reset();
	minimapVerticalGuideSprite_.reset();
	minimapHorizontalGuideSprite_.reset();
	minimapBackgroundSprite_.reset();
	minimapBorderSprite_.reset();
	minimapReady_ = false;
	glassWallVisual_.reset();
	groundVisual_.reset();
	skybox_.reset();
	camera_.reset();
	framework_ = nullptr;
	prototypeReady_ = false;
}

void MagnetPrototypeScene::PrepareFixedUpdate()
{
	pendingCommand_.moveDirection = {};
	pendingCommand_.turnDirection = 0.0f;
	if (!prototypeReady_ || editorMode_ != magnet::MagnetEditorMode::Play) {
		return;
	}
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (!input) { return; }
	const bool rightTriggerPressed = input->GetRightTrigger() >= 0.5f;
	const bool rightTriggerStarted = rightTriggerPressed && !rightTriggerWasPressed_;
	rightTriggerWasPressed_ = rightTriggerPressed;
	if (input->TriggerKey(InputKey::Escape) ||
		input->TriggerGamepadButton(InputGamepadButton::Start)) {
		paused_ = !paused_;
		pauseSelection_ = 0;
		const Vector2 stick = input->GetLeftStick();
		menuStickUpWasPressed_ = stick.y > 0.5f;
		menuStickDownWasPressed_ = stick.y < -0.5f;
		menuStickLeftWasPressed_ = stick.x < -0.5f;
		menuStickRightWasPressed_ = stick.x > 0.5f;
		RefreshGameFlowUi();
		return;
	}
	if (paused_) {
		HandlePauseMenuInput(*input);
		return;
	}
	// Tutorial navigation must remain available after holding Tab. ImGui may
	// retain keyboard capture for a frame after its navigation key is released,
	// so handle these scene controls before honoring UI keyboard capture.
	if (tutorialMode_ && input->TriggerKey(InputKey::Enter)) {
		SkipTutorialPhase();
		return;
	}
	if (tutorialMode_ && tutorialPhase_ == TutorialPhase::TryObstacles &&
		input->TriggerKey(InputKey::Space)) {
		rankingTransitionRequested_ = true;
		SceneManager::GetInstance()->ChangeScene("MAGNET_PROTOTYPE");
		return;
	}
	if (ImGuiManager::GetInstance()->WantsCaptureKeyboard()) { return; }

	if (input->PushKey(InputKey::W)) { pendingCommand_.moveDirection.z += 1.0f; }
	if (input->PushKey(InputKey::S)) { pendingCommand_.moveDirection.z -= 1.0f; }
	if (input->PushKey(InputKey::D)) { pendingCommand_.moveDirection.x += 1.0f; }
	if (input->PushKey(InputKey::A)) { pendingCommand_.moveDirection.x -= 1.0f; }
	const Vector2 leftStick = input->GetLeftStick();
	pendingCommand_.moveDirection.x += leftStick.x;
	pendingCommand_.moveDirection.z += leftStick.y;
	if (input->PushGamepadButton(InputGamepadButton::RightShoulder)) {
		pendingCommand_.turnDirection += 1.0f;
	}
	if (input->PushGamepadButton(InputGamepadButton::LeftShoulder)) {
		pendingCommand_.turnDirection -= 1.0f;
	}
	if (HasMovementInput(pendingCommand_.moveDirection)) {
		releaseOverviewActive_ = false;
	}
	pendingCommand_.emergencyStop =
		pendingCommand_.emergencyStop || input->TriggerKey(InputKey::Space);
	pendingCommand_.releaseChains =
		pendingCommand_.releaseChains || input->TriggerKey(InputKey::Q) ||
		rightTriggerStarted;
	resetRequested_ = resetRequested_ || input->TriggerKey(InputKey::R);
}

void MagnetPrototypeScene::FixedUpdate(float fixedDeltaTime)
{
	if (!prototypeReady_ || editorMode_ != magnet::MagnetEditorMode::Play ||
		paused_ || rankingTransitionRequested_) {
		pendingCommand_ = {};
		return;
	}
	if (!tutorialMode_) { gameElapsedSeconds_ += fixedDeltaTime; }
	if (!tutorialMode_ &&
		gameElapsedSeconds_ >= magnetStageSystem_.GetStageData().timeLimitSeconds) {
		CompleteTimedGame();
		pendingCommand_ = {};
		return;
	}
	if (resetRequested_) {
		prototypeReady_ = magnetChainSystem_.Reset();
		magneticImpactFeedbackSystem_.Reset();
		magneticGoalSoundSystem_.Reset();
		magneticAttachmentSoundSystem_.Reset();
		chainsawCutSoundSystem_.Reset();
		chainsawProximitySoundSystem_.Reset();
		magneticImpactSoundSystem_.Reset();
		furnaceVisualSystem_.Reset();
		magnetStageStructureVisualSystem_.Reset();
		gimmickEffectSystem_.Reset();
		gimmickComicTextSystem_.Reset();
		gimmickSoundSystem_.Reset();
		magnetGimmickVisualSystem_.Reset();
		if (comicTextEffects_) { comicTextEffects_->Clear(); }
		resetRequested_ = false;
		if (!prototypeReady_) {
			Logger::Log("MagnetPrototypeScene: MagnetChainSystem reset failed.");
			assert(false && "MagnetChainSystem reset failed.");
			return;
		}
	}

	const bool releaseWasRequested =
		pendingCommand_.releaseChains && magnetChainSystem_.HasAttachedBalls();
	magneticImpactSoundSystem_.BeginFixedUpdate(fixedDeltaTime);
	magnetChainSystem_.SetPlayerCommand(pendingCommand_);
	prototypeReady_ = magnetChainSystem_.FixedUpdate(fixedDeltaTime);
	if (prototypeReady_) {
		if (furnaceVisualsReady_ && !furnaceVisualSystem_.AddDissolveEvents(
			magnetChainSystem_.GetFurnaceDissolveEvents(),
			magnetChainSystem_.GetFurnaceDissolveEventCount())) {
			Logger::Log(
				"MagnetPrototypeScene: Furnace dissolve event was invalid; disabling its visual path.");
			furnaceVisualsReady_ = false;
		}
		if (magnetChainSystem_.GetAttachmentEvent().occurred) {
			magneticAttachmentSoundSystem_.Play();
		}
		if (magnetChainSystem_.GetGoalEvent().occurred) {
			magneticGoalSoundSystem_.Play();
		}
		if (magnetChainSystem_.GetChainsawCutEvent().occurred) {
			chainsawCutSoundSystem_.Play();
			if (magnetGimmickVisualsReady_) {
				magnetGimmickVisualSystem_.AddChainsawCutEffect(
					magnetStageSystem_.GetStageData(), magnetChainSystem_);
			}
		}
		const auto& wallImpactEvents = magnetChainSystem_.GetWallImpactEvents();
		for (std::size_t index = 0;
			index < magnetChainSystem_.GetWallImpactEventCount();
			++index) {
			if (!magnetChainSystem_.IsActiveUnattachedBallBody(
				wallImpactEvents[index].body)) {
				continue;
			}
			magneticImpactSoundSystem_.AddImpact(
				wallImpactEvents[index].relativeSpeed);
		}
		const auto& arenaImpactEvents = magnetChainSystem_.GetArenaImpactEvents();
		for (std::size_t index = 0;
			index < magnetChainSystem_.GetArenaImpactEventCount();
			++index) {
			if (!magnetChainSystem_.IsActiveUnattachedBallBody(
				arenaImpactEvents[index].body)) {
				continue;
			}
			magneticImpactSoundSystem_.AddImpact(
				arenaImpactEvents[index].relativeSpeed);
		}
		const auto& impactEvents = magnetChainSystem_.GetMagneticImpactEvents();
		const std::size_t impactCount = magnetChainSystem_.GetMagneticImpactEventCount();
		magneticImpactFeedbackSystem_.AddImpacts(
			impactEvents, impactCount);
		if (comicTextEffects_) {
			for (std::size_t index = 0; index < impactCount; ++index) {
				if (impactEvents[index].relativeSpeed >= kComicTextMinimumImpactSpeed) {
					comicTextEffects_->Play(heavyImpactPreset_, impactEvents[index].position);
					magneticImpactSoundSystem_.AddImpact(
						impactEvents[index].relativeSpeed);
				}
			}
		}
		magneticImpactSoundSystem_.PlayPending();
		gimmickSoundSystem_.Update(
			magnetChainSystem_, magnetStageSystem_.GetStageData());
		gimmickEffectSystem_.CaptureEvents(
			magnetChainSystem_, magnetStageSystem_.GetStageData());
		gimmickComicTextSystem_.CaptureEvents(
			magnetChainSystem_, magnetStageSystem_.GetStageData());
		magneticImpactFeedbackSystem_.Update(fixedDeltaTime);
	}
	if (!prototypeReady_) {
		Logger::Log("MagnetPrototypeScene: fixed update failed; simulation disabled.");
		assert(false && "MagnetChainSystem fixed update failed.");
	}
	if (prototypeReady_ && releaseWasRequested &&
		magnetChainSystem_.GetReleasedBallCount() > 0) {
		releaseOverviewActive_ = true;
	}
	pendingCommand_.emergencyStop = false;
	pendingCommand_.releaseChains = false;
	if (prototypeReady_ && tutorialMode_) { UpdateTutorialProgress(fixedDeltaTime); }
}

void MagnetPrototypeScene::Update()
{
	const FrameClock* frameClock = framework_ ? framework_->GetFrameClock() : nullptr;
	const float frameDeltaSeconds = frameClock
		? frameClock->GetFrameDeltaSeconds()
		: FrameClock::kDefaultFixedDeltaSeconds;
	HandleStageEditorKeyboardInput(frameDeltaSeconds);
	const bool chainsawSoundEnabled = prototypeReady_ &&
		editorMode_ == magnet::MagnetEditorMode::Play &&
		!paused_ && !rankingTransitionRequested_;
	const physics::SphereBody* playerBody = chainsawSoundEnabled
		? magnetChainSystem_.GetPhysicsWorld().GetBody(
			magnetChainSystem_.GetPlayerBody())
		: nullptr;
	const magnet::MagnetStageData& stageData = magnetStageSystem_.GetStageData();
	const float arenaVisualScale = stageData.arenaRadius / kAuthoredArenaRadius;
	if (groundVisual_) {
		groundVisual_->SetScale({ arenaVisualScale, arenaVisualScale, arenaVisualScale });
	}
	if (glassWallVisual_) {
		glassWallVisual_->SetScale({ arenaVisualScale, arenaVisualScale, arenaVisualScale });
	}
	chainsawProximitySoundSystem_.Update(
		frameDeltaSeconds,
		playerBody != nullptr,
		playerBody ? playerBody->position : Vector3{},
		stageData.obstacles.data(),
		stageData.obstacleCount);
	if (camera_) {
		Vector3 desiredPosition = camera_->GetTranslate();
		bool shouldMoveCamera = false;
		if (prototypeReady_ && editorMode_ == magnet::MagnetEditorMode::StageEdit) {
			const Vector3 focus = ResolveEditorFocusPosition();
			shouldMoveCamera = magnetEditorCameraSystem_.TryCalculatePosition(
				focus,
				desiredPosition);
		} else if (prototypeReady_ && releaseOverviewActive_) {
			desiredPosition = CalculatePlayCameraPosition();
			shouldMoveCamera = IsFiniteVector3(desiredPosition);
		} else if (prototypeReady_ && cameraFollow_) {
			desiredPosition = CalculatePlayCameraPosition();
			shouldMoveCamera = IsFiniteVector3(desiredPosition);
		}
		if (shouldMoveCamera) {
			const Vector3 currentPosition = camera_->GetTranslate();
			camera_->SetTranslate(
				currentPosition + (desiredPosition - currentPosition) * kCameraBlend +
				(editorMode_ == magnet::MagnetEditorMode::Play
					? magneticImpactFeedbackSystem_.GetCameraShakeOffset()
					: Vector3{}));
		}
		camera_->Update();
		if (skybox_) { skybox_->Update(camera_.get()); }
		if (groundVisual_) { groundVisual_->Update(camera_.get(), 0.0f); }
		if (glassWallVisual_) { glassWallVisual_->Update(camera_.get(), 0.0f); }
	}
	if (ballVisualsReady_ && !UpdateBallVisuals(frameDeltaSeconds)) {
		Logger::Log(
			"MagnetPrototypeScene: ball visual update failed; using wire fallback.");
		ballVisualsReady_ = false;
	}
	if (tutorialMode_) { UpdateTutorialUi(); }
	if (furnaceVisualsReady_ && !furnaceVisualSystem_.Update(
		frameDeltaSeconds, stageData, camera_.get())) {
		Logger::Log(
			"MagnetPrototypeScene: Furnace visual update failed; using wire fallback.");
		furnaceVisualsReady_ = false;
	}
	if (stageStructureVisualsReady_ &&
		!magnetStageStructureVisualSystem_.Update(
			frameDeltaSeconds, stageData, camera_.get(), &magnetChainSystem_)) {
		Logger::Log(
			"MagnetPrototypeScene: Goal/Wall visual update failed; using wire fallback.");
		stageStructureVisualsReady_ = false;
	}
	if (magnetGimmickVisualsReady_ && !magnetGimmickVisualSystem_.Update(
		frameDeltaSeconds, stageData, magnetChainSystem_, camera_.get())) {
		Logger::Log(
			"MagnetPrototypeScene: Transfer/repulsion/anchor visual update failed; "
			"using wire fallback.");
		magnetGimmickVisualsReady_ = false;
	}
	if (framework_ && framework_->GetParticleManager() && camera_) {
		framework_->GetParticleManager()->Update(camera_.get(), frameDeltaSeconds);
	}
	gimmickEffectSystem_.Update(frameDeltaSeconds);
	if (comicTextEffects_ && camera_) {
		gimmickComicTextSystem_.Update(frameDeltaSeconds);
		comicTextEffects_->Update(
			frameDeltaSeconds,
			camera_->GetViewProjectionMatrix());
	}
	UpdateGoalGuides();
	UpdateMinimap();
	RefreshGameFlowUi();
}

void MagnetPrototypeScene::DrawEditorUi(const SceneEditorContext& context)
{
	magnet::MagnetPrototypeViewData viewData{};
	viewData.healthy = prototypeReady_ && magnetChainSystem_.IsHealthy();
	viewData.bodyCount = magnetChainSystem_.GetPhysicsWorld().GetBodyCount();
	viewData.constraintCount = magnetChainSystem_.GetPhysicsWorld().GetConstraints().size();
	viewData.activeConstraintCount =
		magnetChainSystem_.GetPhysicsWorld().GetActiveConstraintCount();
	viewData.availableBallCount = magnetChainSystem_.GetAvailableBallCount();
	viewData.attachedBallCount = magnetChainSystem_.GetAttachedBallCount();
	viewData.releasedBallCount = magnetChainSystem_.GetReleasedBallCount();
	viewData.leftChainCount = magnetChainSystem_.GetLeftChainCount();
	viewData.rightChainCount = magnetChainSystem_.GetRightChainCount();
	viewData.maximumConstraintError = magnetChainSystem_.GetMaximumConstraintError();
	viewData.attachmentRadius = magnet::MagnetChainSystem::GetAttachmentRadius();
	viewData.spinChargeRatio = magnetChainSystem_.GetSpinChargeRatio();
	viewData.spinChargeRotations = magnetChainSystem_.GetSpinChargeRotationRadians() / 6.28318530717958647692f;
	viewData.spinChargeSpeedMultiplier = magnetChainSystem_.GetSpinChargeSpeedMultiplier();
	viewData.spinChargeTurnSpeedMultiplier = magnetChainSystem_.GetSpinChargeTurnSpeedMultiplier();
	viewData.magneticAttachmentCount = magnetChainSystem_.GetMagneticAttachmentCount();
	viewData.goalHitCount = magnetChainSystem_.GetGoalHitCount();
	viewData.score = magnetChainSystem_.GetScore();
	viewData.scoreNumberTextureSrvIndex = scoreNumberTextureSrvIndex_;
	viewData.goalWidth = magnetChainSystem_.GetGoal().width;
	viewData.stageData = &magnetStageSystem_.GetStageData();
	viewData.saveEntries = magnetStageSystem_.GetSaveEntries().data();
	viewData.saveEntryCount = magnetStageSystem_.GetSaveEntryCount();
	viewData.stageOperationMessage = magnetStageSystem_.GetLastOperationMessage().c_str();
	viewData.stageOperationSucceeded = magnetStageSystem_.DidLastOperationSucceed();
	viewData.stageDirty = magnetStageSystem_.IsDirty();
	viewData.editorMode = editorMode_;
	const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
		magnetChainSystem_.GetPlayerBody());
	if (player) {
		viewData.playerSpeed = std::sqrt(
			player->linearVelocity.x * player->linearVelocity.x +
			player->linearVelocity.z * player->linearVelocity.z);
	}

	const magnet::MagnetPrototypeUiRequest request = prototypeWindow_.Draw(
		viewData,
		context.srvManager,
		context.finalDisplaySrvIndex,
		context.virtualWidth,
		context.virtualHeight);
#ifdef USE_IMGUI
	if (particleEffectEditor_ && framework_ && framework_->GetParticleManager()) {
		Vector3 effectPosition = ResolveEditorFocusPosition();
		effectPosition.y += 0.5f;
		if (ImGui::Begin("エフェクトエディタ###ParticleEffectEditor")) {
			if (ImGui::Button("文字エフェクトエディタを開く")) {
				SceneManager::GetInstance()->ChangeScene("EFFECT_EDITOR");
			}
			ImGui::SameLine();
			ImGui::TextDisabled("プリセットの作成・変更・削除");
			ImGui::Separator();
			const FrameClock* frameClock = framework_->GetFrameClock();
			particleEffectEditor_->Draw(*framework_->GetParticleManager(), effectPosition,
				frameClock ? frameClock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds);
		}
		ImGui::End();
	}
#endif
	if (request.modeChangeRequested) {
		SetEditorMode(request.requestedMode);
	}
	selectedObjectType_ = request.selectedObjectType;
	selectedObjectId_ = request.selectedObjectId;
	if (editorMode_ == magnet::MagnetEditorMode::StageEdit &&
		request.editorViewportClickRequested) {
		SelectStageObjectAtNdc(request.editorViewportClickNdc);
	}
	if (editorMode_ == magnet::MagnetEditorMode::StageEdit &&
		request.editorCameraZoomWheelDelta != 0.0f &&
		!magnetEditorCameraSystem_.ApplyWheelDelta(
			request.editorCameraZoomWheelDelta)) {
		magnetEditorCameraSystem_.Reset();
		Logger::Log(
			"MagnetPrototypeScene: invalid editor camera wheel input was rejected.");
	}
	if (editorMode_ == magnet::MagnetEditorMode::StageEdit &&
		!magnetEditorCameraSystem_.ApplyPanDrag(
			request.editorCameraPanDragDelta)) {
		magnetEditorCameraSystem_.Reset();
		Logger::Log(
			"MagnetPrototypeScene: invalid editor camera drag input was rejected.");
	}
	if (editorMode_ == magnet::MagnetEditorMode::Play) {
		resetRequested_ = resetRequested_ || request.reset;
		pendingCommand_.emergencyStop =
			pendingCommand_.emergencyStop || request.emergencyStop;
		pendingCommand_.releaseChains =
			pendingCommand_.releaseChains || request.releaseChains;
	}
	ProcessStageEditorRequest(request);
	ValidateEditorSelection();
	magnetChainSystem_.SetSpinChargeSettings(request.spinChargeSettings);
	magnetChainSystem_.SetImpactAttachmentSettings(request.impactAttachmentSettings);
	showGrid_ = request.showGrid;
	showVelocity_ = request.showVelocity;
	cameraFollow_ = request.cameraFollow;
}

void MagnetPrototypeScene::Draw()
{
	if (!camera_ || !prototypeReady_) {
		return;
	}

	// The sky is the background, so render it before particles and screen-space effects.
	// Drawing it later would overwrite effects that do not write to the depth buffer.
	if (skybox_) { skybox_->Draw(); }
	if (groundVisual_) {
		Object3dCommon* objectCommon = framework_->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		groundVisual_->Draw();
		objectCommon->EndObjectPass();
	}

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	const float arenaRadius = magnetChainSystem_.GetArenaRadius();
	if (showGrid_) {
		const int gridHalfCount = static_cast<int>(std::ceil(arenaRadius / kGridSpacing));
		for (int index = -gridHalfCount; index <= gridHalfCount; ++index) {
			const float offset = static_cast<float>(index) * kGridSpacing;
			if (std::abs(offset) > arenaRadius) {
				continue;
			}
			const float halfChord = std::sqrt(
				(std::max)(0.0f, arenaRadius * arenaRadius - offset * offset));
			lineDrawer->DrawLine(
				{ -halfChord, 0.0f, offset },
				{ halfChord, 0.0f, offset },
				kGridColor);
			lineDrawer->DrawLine(
				{ offset, 0.0f, -halfChord },
				{ offset, 0.0f, halfChord },
				kGridColor);
		}
	}
	if (!glassWallVisual_) {
		for (int segment = 0; segment < kArenaWallSegments; ++segment) {
			const float firstAngle = 6.28318530717958647692f *
				static_cast<float>(segment) / static_cast<float>(kArenaWallSegments);
			const float secondAngle = 6.28318530717958647692f *
				static_cast<float>(segment + 1) / static_cast<float>(kArenaWallSegments);
			const Vector3 bottomA = { std::cos(firstAngle) * arenaRadius, 0.0f, std::sin(firstAngle) * arenaRadius };
			const Vector3 bottomB = { std::cos(secondAngle) * arenaRadius, 0.0f, std::sin(secondAngle) * arenaRadius };
			const Vector3 topA = bottomA + Vector3{ 0.0f, kArenaWallHeight, 0.0f };
			const Vector3 topB = bottomB + Vector3{ 0.0f, kArenaWallHeight, 0.0f };
			lineDrawer->DrawLine(bottomA, bottomB, kConstraintColor);
			lineDrawer->DrawLine(topA, topB, kConstraintColor);
			if (segment % 4 == 0) {
				lineDrawer->DrawLine(bottomA, topA, kConstraintColor);
			}
		}
	}

	const physics::PhysicsWorld& physicsWorld = magnetChainSystem_.GetPhysicsWorld();
	for (const physics::DistanceConstraint& constraint : physicsWorld.GetConstraints()) {
		if (!constraint.active || !constraint.debugDraw) {
			continue;
		}
		const physics::SphereBody* bodyA = physicsWorld.GetBody(constraint.bodyA);
		const physics::SphereBody* bodyB = physicsWorld.GetBody(constraint.bodyB);
		if (bodyA && bodyB && bodyA->active && bodyB->active) {
			lineDrawer->DrawLine(bodyA->position, bodyB->position, kConstraintColor);
		}
	}

	DrawBody(magnetChainSystem_.GetPlayerBody(), kPlayerColor);
	DrawVelocity(magnetChainSystem_.GetPlayerBody());
	const auto& stageBalls = magnetChainSystem_.GetStageBalls();
	const auto& stageBallStates = magnetChainSystem_.GetStageBallStates();
	for (std::size_t index = 0; index < magnetChainSystem_.GetStageBallCount(); ++index) {
		if (stageBallStates[index] ==
			magnet::MagnetChainSystem::StageBallState::Inactive) {
			continue;
		}
		DrawBody(stageBalls[index], GetStageBallColor(stageBallStates[index]));
		DrawVelocity(stageBalls[index]);
	}
	magneticImpactFeedbackSystem_.Draw(*lineDrawer);
	gimmickEffectSystem_.Draw(*lineDrawer);
	DrawStageObjects();
	DrawSelectionHighlight();
	magnet::MagnetStageData visibleStageData = magnetStageSystem_.GetStageData();
	if (tutorialMode_) {
		if (tutorialPhase_ < TutorialPhase::ScoreGoal) { visibleStageData.goalCount = 0; }
		if (tutorialPhase_ < TutorialPhase::TryObstacles) { visibleStageData.obstacleCount = 0; }
	}
	if (stageStructureVisualsReady_) {
		magnetStageStructureVisualSystem_.Draw(visibleStageData);
	}
	if (furnaceVisualsReady_) {
		furnaceVisualSystem_.Draw(visibleStageData);
	}
	if (magnetGimmickVisualsReady_) {
		magnetGimmickVisualSystem_.Draw(
			visibleStageData, magnetChainSystem_);
	}
	DrawBallVisuals();
	if (glassWallVisual_) {
		Object3dCommon* objectCommon = framework_->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		glassWallVisual_->Draw();
		objectCommon->EndObjectPass();
	}
	if (framework_ && framework_->GetParticleManager()) {
		framework_->GetParticleManager()->Draw();
	}

	lineDrawer->Draw(camera_->GetViewProjectionMatrix());
	if (comicTextEffects_) { comicTextEffects_->Draw(); }
	DrawGoalGuides();
	DrawMinimap();
	DrawGameFlowUi();
}

bool MagnetPrototypeScene::InitializeGameFlowUi()
{
	SpriteCommon* spriteCommon = framework_ ? framework_->GetSpriteCommon() : nullptr;
	if (!spriteCommon || !gameFlowFont_.InitializeFromJson(
		spriteCommon, "Resources/ui/font/ascii_bitmap_font.json")) {
		return false;
	}
	const Texture2DHandle scoreNumberTexture =
		TextureManager::GetInstance()->LoadTexture2D("Resources/ui/numbers.png");
	scoreNumberTextureSrvIndex_ = scoreNumberTexture.IsValid()
		? scoreNumberTexture.Index()
		: UINT32_MAX;
	timerDigitsReady_ = false;
	pauseOverlaySprite_ = std::make_unique<Sprite>();
	if (!pauseOverlaySprite_->Initialize(spriteCommon, "Resources/human/white.png")) {
		pauseOverlaySprite_.reset();
		return false;
	}
	pauseOverlaySprite_->SetPosition({ 310.0f, 95.0f });
	pauseOverlaySprite_->SetSize({ 660.0f, 530.0f });
	pauseOverlaySprite_->SetColor({ 0.015f, 0.03f, 0.055f, 0.94f });
	pauseOverlaySprite_->Update();

	ModelManager* modelManager = framework_->GetModelManager();
	if (modelManager) {
		pauseLabelCamera_ = std::make_unique<Camera>();
		pauseLabelCamera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
		pauseLabelCamera_->SetRotate({ 0.0f, 0.0f, 0.0f });
		pauseLabelCamera_->Update();

		constexpr const char* kScoreHudModelPath = "title/Score.obj";
		modelManager->LoadModel(kScoreHudModelPath);
		Model* scoreHudModel = modelManager->GetModel(kScoreHudModelPath);
		if (scoreHudModel) {
			scoreHudModel->LoadTextures();
			scoreHudObject_ = std::make_unique<Object3d>();
			scoreHudObject_->Initialize(framework_->GetObject3dCommon());
			scoreHudObject_->SetModel(scoreHudModel);
			scoreHudObject_->SetScale({ 0.22f, 0.22f, 0.22f });
			scoreHudObject_->SetPosition({ -3.58f, 1.98f, 0.0f });
			scoreHudObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
			scoreHudObject_->SetColor(kUiAccentColor);
			scoreHudObject_->SetEnableLighting(false);
			scoreHudObject_->SetCullMode(0);
			scoreHudObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kTimeHudModelPath = "ui/time/Time.obj";
		modelManager->LoadModel(kTimeHudModelPath);
		Model* timeHudModel = modelManager->GetModel(kTimeHudModelPath);
		if (timeHudModel) {
			timeHudModel->LoadTextures();
			timeHudObject_ = std::make_unique<Object3d>();
			timeHudObject_->Initialize(framework_->GetObject3dCommon());
			timeHudObject_->SetModel(timeHudModel);
			timeHudObject_->SetScale({ 0.26f, 0.26f, 0.26f });
			timeHudObject_->SetPosition({ -0.55f, 1.95f, 0.0f });
			timeHudObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
			timeHudObject_->SetColor(kUiAccentColor);
			timeHudObject_->SetEnableLighting(false);
			timeHudObject_->SetCullMode(0);
			timeHudObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kPauseTitleModelPath = "pause/pause.obj";
		modelManager->LoadModel(kPauseTitleModelPath);
		Model* pauseTitleModel = modelManager->GetModel(kPauseTitleModelPath);
		if (pauseTitleModel) {
			pauseTitleModel->LoadTextures();
			pauseTitleObject_ = std::make_unique<Object3d>();
			pauseTitleObject_->Initialize(framework_->GetObject3dCommon());
			pauseTitleObject_->SetModel(pauseTitleModel);
			pauseTitleObject_->SetScale({ 0.035f, 0.035f, 0.035f });
			pauseTitleObject_->SetPosition({ 0.0f, 0.115f, -9.0f });
			pauseTitleObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
			pauseTitleObject_->SetColor({ 0.32f, 0.95f, 1.0f, 1.0f });
			pauseTitleObject_->SetEnableLighting(false);
			pauseTitleObject_->SetCullMode(0);
			pauseTitleObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kResumeModelPath = "pause/resume.obj";
		modelManager->LoadModel(kResumeModelPath);
		Model* resumeModel = modelManager->GetModel(kResumeModelPath);
		if (resumeModel) {
			resumeModel->LoadTextures();
			resumeLabelObject_ = std::make_unique<Object3d>();
			resumeLabelObject_->Initialize(framework_->GetObject3dCommon());
			resumeLabelObject_->SetModel(resumeModel);
			resumeLabelObject_->SetScale({ 0.017f, 0.017f, 0.017f });
			resumeLabelObject_->SetPosition({ -0.0013f, 0.0685f, -9.0f });
			resumeLabelObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
			resumeLabelObject_->SetEnableLighting(false);
			resumeLabelObject_->SetCullMode(0);
			resumeLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kRestartModelPath = "pause/restart.obj";
		modelManager->LoadModel(kRestartModelPath);
		Model* restartModel = modelManager->GetModel(kRestartModelPath);
		if (restartModel) {
			restartModel->LoadTextures();
			restartLabelObject_ = std::make_unique<Object3d>();
			restartLabelObject_->Initialize(framework_->GetObject3dCommon());
			restartLabelObject_->SetModel(restartModel);
			restartLabelObject_->SetScale({ 0.022f, 0.022f, 0.022f });
			restartLabelObject_->SetPosition({ 0.0053f, 0.0175f, -9.0f });
			restartLabelObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
			restartLabelObject_->SetEnableLighting(false);
			restartLabelObject_->SetCullMode(0);
			restartLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kBackTitleModelPath = "pause/BackTitle.obj";
		modelManager->LoadModel(kBackTitleModelPath);
		Model* backTitleModel = modelManager->GetModel(kBackTitleModelPath);
		if (backTitleModel) {
			backTitleModel->LoadTextures();
			backTitleLabelObject_ = std::make_unique<Object3d>();
			backTitleLabelObject_->Initialize(framework_->GetObject3dCommon());
			backTitleLabelObject_->SetModel(backTitleModel);
			backTitleLabelObject_->SetScale({ 0.062f, 0.062f, 0.062f });
			backTitleLabelObject_->SetPosition({ 0.0028f, -0.0325f, -9.0f });
			backTitleLabelObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
			backTitleLabelObject_->SetEnableLighting(false);
			backTitleLabelObject_->SetCullMode(0);
			backTitleLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kVolumeModelPath = "pause/Volume.obj";
		modelManager->LoadModel(kVolumeModelPath);
		Model* volumeModel = modelManager->GetModel(kVolumeModelPath);
		if (volumeModel) {
			volumeModel->LoadTextures();
			volumeLabelObject_ = std::make_unique<Object3d>();
			volumeLabelObject_->Initialize(framework_->GetObject3dCommon());
			volumeLabelObject_->SetModel(volumeModel);
			volumeLabelObject_->SetScale({ 0.053f, 0.053f, 0.053f });
			volumeLabelObject_->SetPosition({ -0.025f, -0.083f, -9.0f });
			volumeLabelObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
			volumeLabelObject_->SetEnableLighting(false);
			volumeLabelObject_->SetCullMode(0);
			volumeLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}

		constexpr const char* kSeVolumeModelPath = "pause/Se.obj";
		modelManager->LoadModel(kSeVolumeModelPath);
		Model* seVolumeModel = modelManager->GetModel(kSeVolumeModelPath);
		if (seVolumeModel) {
			seVolumeModel->LoadTextures();
			seVolumeLabelObject_ = std::make_unique<Object3d>();
			seVolumeLabelObject_->Initialize(framework_->GetObject3dCommon());
			seVolumeLabelObject_->SetModel(seVolumeModel);
			seVolumeLabelObject_->SetScale({ 0.022f, 0.022f, 0.022f });
			seVolumeLabelObject_->SetPosition({ -0.035f, -0.133f, -9.0f });
			seVolumeLabelObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
			seVolumeLabelObject_->SetEnableLighting(false);
			seVolumeLabelObject_->SetCullMode(0);
			seVolumeLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}
	}

	const auto initializeText = [spriteCommon, this](SpriteText& text) {
		text.Initialize(spriteCommon, &gameFlowFont_);
		text.SetCharacterSpacing(-6.0f);
	};
	initializeText(timerText_);
	initializeText(pauseTitleText_);
	for (SpriteText& text : pauseMenuTexts_) { initializeText(text); }
	initializeText(pauseHelpText_);

	timerText_.SetPosition({ 595.0f, 28.0f });
	timerText_.SetScale(1.0f);
	timerText_.SetColor(kUiAccentColor);
	pauseTitleText_.SetText(pauseTitleObject_ ? "" : "PAUSE");
	pauseTitleText_.SetPosition({ 535.0f, 135.0f });
	pauseTitleText_.SetScale(1.55f);
	pauseTitleText_.SetColor({ 0.32f, 0.95f, 1.0f, 1.0f });
	pauseTitleText_.Update();
	for (std::size_t index = 0; index < pauseMenuTexts_.size(); ++index) {
		pauseMenuTexts_[index].SetPosition(
			{ 445.0f, 255.0f + 65.0f * static_cast<float>(index) });
		pauseMenuTexts_[index].SetScale(1.05f);
	}
	if (volumeLabelObject_) {
		pauseMenuTexts_[3].SetPosition({ 710.0f, 455.0f });
		pauseMenuTexts_[3].SetScale(1.75f);
	}
	if (seVolumeLabelObject_) {
		pauseMenuTexts_[4].SetPosition({ 660.0f, 536.0f });
		pauseMenuTexts_[4].SetScale(1.75f);
	}
	pauseHelpText_.SetText("");
	return true;
}

void MagnetPrototypeScene::HandlePauseMenuInput(Input& input)
{
	const Vector2 stick = input.GetLeftStick();
	const bool stickUp = stick.y > 0.5f;
	const bool stickDown = stick.y < -0.5f;
	const bool stickLeft = stick.x < -0.5f;
	const bool stickRight = stick.x > 0.5f;
	const bool moveUp = input.TriggerKey(InputKey::W) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadUp) ||
		(stickUp && !menuStickUpWasPressed_);
	const bool moveDown = input.TriggerKey(InputKey::S) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadDown) ||
		(stickDown && !menuStickDownWasPressed_);
	const bool moveLeft = input.TriggerKey(InputKey::A) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadLeft) ||
		(stickLeft && !menuStickLeftWasPressed_);
	const bool moveRight = input.TriggerKey(InputKey::D) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadRight) ||
		(stickRight && !menuStickRightWasPressed_);
	menuStickUpWasPressed_ = stickUp;
	menuStickDownWasPressed_ = stickDown;
	menuStickLeftWasPressed_ = stickLeft;
	menuStickRightWasPressed_ = stickRight;

	if (moveUp) {
		pauseSelection_ = (pauseSelection_ + kPauseMenuItemCount - 1) % kPauseMenuItemCount;
	}
	if (moveDown) {
		pauseSelection_ = (pauseSelection_ + 1) % kPauseMenuItemCount;
	}
	if (pauseSelection_ == 3) {
		float volume = GameFlowState::GetInstance().GetBgmVolume();
		if (moveLeft) { volume -= 0.1f; }
		if (moveRight) { volume += 0.1f; }
		GameFlowState::GetInstance().SetBgmVolume(volume);
	} else if (pauseSelection_ == 4) {
		float volume = GameFlowState::GetInstance().GetSeVolume();
		if (moveLeft) { volume -= 0.1f; }
		if (moveRight) { volume += 0.1f; }
		GameFlowState::GetInstance().SetSeVolume(volume);
	}
	if (input.TriggerKey(InputKey::Space) ||
		input.TriggerGamepadButton(InputGamepadButton::B)) {
		if (pauseSelection_ == 0) {
			paused_ = false;
		} else if (pauseSelection_ == 1) {
			rankingTransitionRequested_ = true;
			SceneManager::GetInstance()->ChangeScene("MAGNET_PROTOTYPE");
		} else if (pauseSelection_ == 2) {
			rankingTransitionRequested_ = true;
			SceneManager::GetInstance()->ChangeScene("TITLE");
		}
	}
	RefreshGameFlowUi();
}

void MagnetPrototypeScene::RefreshGameFlowUi()
{
	if (!gameFlowUiReady_) { return; }
	char timerBuffer[32]{};
	const int remainingSeconds = static_cast<int>(std::ceil(
		(std::max)(0.0f,
			magnetStageSystem_.GetStageData().timeLimitSeconds - gameElapsedSeconds_)));
	std::snprintf(timerBuffer, sizeof(timerBuffer), "%02d", remainingSeconds);
	timerText_.SetText(timerBuffer);
	timerText_.Update();
	if (timerDigitsReady_) {
		const std::array<int, 2> digits = {
			(remainingSeconds / 10) % 10, remainingSeconds % 10
		};
		for (std::size_t index = 0; index < timerDigitSprites_.size(); ++index) {
			const int atlasIndex = digits[index] == 0 ? 9 : digits[index] - 1;
			timerDigitSprites_[index]->SetTextureRect(
				{ 16.0f * static_cast<float>(atlasIndex), 0.0f },
				{ 16.0f, 16.0f });
			timerDigitSprites_[index]->Update();
		}
	}
	if (!paused_) { return; }

	const int volumePercent = static_cast<int>(std::lround(
		GameFlowState::GetInstance().GetBgmVolume() * 100.0f));
	const int seVolumePercent = static_cast<int>(std::lround(
		GameFlowState::GetInstance().GetSeVolume() * 100.0f));
	const char* fixedLabels[] = {
		resumeLabelObject_ ? "" : "BACK TO GAME",
		restartLabelObject_ ? "" : "RESTART",
		backTitleLabelObject_ ? "" : "BACK TO TITLE"
	};
	for (int index = 0; index < kPauseMenuItemCount; ++index) {
		char label[64]{};
		if (index == 3) {
			if (volumeLabelObject_) {
				std::snprintf(label, sizeof(label), "%d%%", volumePercent);
			} else {
				std::snprintf(label, sizeof(label), "%s BGM VOLUME %d%%",
					index == pauseSelection_ ? ">" : " ", volumePercent);
			}
		} else if (index == 4) {
			if (seVolumeLabelObject_) {
				std::snprintf(label, sizeof(label), "%d%%", seVolumePercent);
			} else {
				std::snprintf(label, sizeof(label), "%s SE VOLUME %d%%",
					index == pauseSelection_ ? ">" : " ", seVolumePercent);
			}
		} else {
			std::snprintf(label, sizeof(label), "%s %s",
				index == pauseSelection_ ? ">" : " ", fixedLabels[index]);
		}
		pauseMenuTexts_[index].SetText(label);
		pauseMenuTexts_[index].SetColor(
			index == pauseSelection_ ? kUiAccentColor : kUiTextColor);
		pauseMenuTexts_[index].Update();
	}
	if (resumeLabelObject_) {
		resumeLabelObject_->SetColor(
			pauseSelection_ == 0 ? kUiAccentColor : kUiTextColor);
		resumeLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
	}
	if (restartLabelObject_) {
		restartLabelObject_->SetColor(
			pauseSelection_ == 1 ? kUiAccentColor : kUiTextColor);
		restartLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
	}
	if (backTitleLabelObject_) {
		backTitleLabelObject_->SetColor(
			pauseSelection_ == 2 ? kUiAccentColor : kUiTextColor);
		backTitleLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
	}
	if (volumeLabelObject_) {
		volumeLabelObject_->SetColor(
			pauseSelection_ == 3 ? kUiAccentColor : kUiTextColor);
		volumeLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
	}
	if (seVolumeLabelObject_) {
		seVolumeLabelObject_->SetColor(
			pauseSelection_ == 4 ? kUiAccentColor : kUiTextColor);
		seVolumeLabelObject_->Update(pauseLabelCamera_.get(), 0.0f);
	}
}

void MagnetPrototypeScene::DrawGameFlowUi()
{
	if (!gameFlowUiReady_) { return; }
	if (scoreHudObject_ || (!tutorialMode_ && timeHudObject_)) {
		Object3dCommon* objectCommon = framework_->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		if (scoreHudObject_) { scoreHudObject_->Draw(); }
		if (!tutorialMode_ && timeHudObject_) { timeHudObject_->Draw(); }
		objectCommon->EndObjectPass();
	}
	framework_->GetSpriteCommon()->PreDraw();
	if (!tutorialMode_) {
		if (timeHudObject_ && timerDigitsReady_) {
			for (auto& sprite : timerDigitSprites_) { sprite->Draw(); }
		} else {
			timerText_.Draw();
		}
	}
	if (tutorialMode_) {
		DrawTutorialUi();
		DrawTutorialSkipUi();
	}
	if (!paused_) { return; }
	pauseOverlaySprite_->Draw();
	pauseTitleText_.Draw();
	if (pauseTitleObject_ || resumeLabelObject_ || restartLabelObject_ ||
		backTitleLabelObject_ || volumeLabelObject_ || seVolumeLabelObject_) {
		Object3dCommon* objectCommon = framework_->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		if (pauseTitleObject_) { pauseTitleObject_->Draw(); }
		if (resumeLabelObject_) { resumeLabelObject_->Draw(); }
		if (restartLabelObject_) { restartLabelObject_->Draw(); }
		if (backTitleLabelObject_) { backTitleLabelObject_->Draw(); }
		if (volumeLabelObject_) { volumeLabelObject_->Draw(); }
		if (seVolumeLabelObject_) { seVolumeLabelObject_->Draw(); }
		objectCommon->EndObjectPass();
		framework_->GetSpriteCommon()->PreDraw();
	}
	for (SpriteText& text : pauseMenuTexts_) { text.Draw(); }
}

bool MagnetPrototypeScene::InitializeTutorialUi()
{
	SpriteCommon* spriteCommon = framework_ ? framework_->GetSpriteCommon() : nullptr;
	if (!spriteCommon) { return false; }
	ModelManager* modelManager = framework_->GetModelManager();
	if (modelManager && pauseLabelCamera_) {
		constexpr const char* kSpaceModelPath = "title/Space.obj";
		modelManager->LoadModel(kSpaceModelPath);
		Model* spaceModel = modelManager->GetModel(kSpaceModelPath);
		if (spaceModel) {
			spaceModel->LoadTextures();
			tutorialTransitionKeyObject_ = std::make_unique<Object3d>();
			tutorialTransitionKeyObject_->Initialize(framework_->GetObject3dCommon());
			tutorialTransitionKeyObject_->SetModel(spaceModel);
			tutorialTransitionKeyObject_->SetScale({ 0.60f, 0.60f, 0.60f });
			tutorialTransitionKeyObject_->SetPosition({ 0.147f, -2.17f, 0.0f });
			tutorialTransitionKeyObject_->SetRotation(
				{ 0.0f, 3.14159265f, 0.0f });
			tutorialTransitionKeyObject_->SetColor(kUiAccentColor);
			tutorialTransitionKeyObject_->SetEnableLighting(false);
			tutorialTransitionKeyObject_->SetCullMode(0);
			tutorialTransitionKeyObject_->Update(pauseLabelCamera_.get(), 0.0f);
		}
	}
	for (auto& sprite : tutorialKeySprites_) {
		sprite = std::make_unique<Sprite>();
		if (!sprite->Initialize(spriteCommon, "Resources/human/white.png")) {
			return false;
		}
		sprite->SetSize({ 54.0f, 54.0f });
	}
	const std::array<Vector2, 4> positions = {
		Vector2{ 82.0f, 584.0f }, Vector2{ 28.0f, 638.0f },
		Vector2{ 82.0f, 638.0f }, Vector2{ 136.0f, 638.0f }
	};
	const std::array<const char*, 4> textures = {
		"Resources/ui/tutorial/W.png", "Resources/ui/tutorial/A.png",
		"Resources/ui/tutorial/S.png", "Resources/ui/tutorial/D.png"
	};
	for (std::size_t index = 0; index < tutorialKeySprites_.size(); ++index) {
		tutorialKeySprites_[index]->SetPosition(
			{ positions[index].x - 3.0f, positions[index].y - 3.0f });
		tutorialKeySprites_[index]->Update();
		tutorialKeyImageSprites_[index] = std::make_unique<Sprite>();
		if (!tutorialKeyImageSprites_[index]->Initialize(
			spriteCommon, textures[index])) {
			return false;
		}
		tutorialKeyImageSprites_[index]->SetPosition(positions[index]);
		tutorialKeyImageSprites_[index]->SetSize({ 48.0f, 48.0f });
		tutorialKeyImageSprites_[index]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		tutorialKeyImageSprites_[index]->Update();
	}
	tutorialMoveGuideSprite_ = std::make_unique<Sprite>();
	if (!tutorialMoveGuideSprite_->Initialize(
		spriteCommon, "Resources/ui/tutorial/GuideMove.png")) {
		return false;
	}
	tutorialMoveGuideSprite_->SetPosition({ 28.0f, 545.0f });
	// The source image has a few isolated pixels along its top edge. Crop those
	// pixels so scaling the guide does not turn them into a visible horizontal line.
	tutorialMoveGuideSprite_->SetTextureRect({ 0.0f, 3.0f }, { 150.0f, 13.0f });
	tutorialMoveGuideSprite_->SetSize({ 300.0f, 26.0f });
	tutorialMoveGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	tutorialMoveGuideSprite_->Update();
	tutorialAttractGuideSprite_ = std::make_unique<Sprite>();
	if (!tutorialAttractGuideSprite_->Initialize(
		spriteCommon, "Resources/ui/tutorial/attractGuide.png")) {
		return false;
	}
	tutorialAttractGuideSprite_->SetPosition({ 28.0f, 545.0f });
	tutorialAttractGuideSprite_->SetSize({ 240.0f, 32.0f });
	tutorialAttractGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	tutorialAttractGuideSprite_->Update();
	tutorialShootGuideSprite_ = std::make_unique<Sprite>();
	if (!tutorialShootGuideSprite_->Initialize(
		spriteCommon, "Resources/ui/tutorial/ShootGuide.png")) {
		return false;
	}
	tutorialShootGuideSprite_->SetPosition({ 28.0f, 545.0f });
	// Crop isolated pixels at the top edge so they are not enlarged into a line.
	tutorialShootGuideSprite_->SetTextureRect({ 0.0f, 3.0f }, { 120.0f, 13.0f });
	tutorialShootGuideSprite_->SetSize({ 240.0f, 26.0f });
	tutorialShootGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	tutorialShootGuideSprite_->Update();
	tutorialGimmickGuideSprite_ = std::make_unique<Sprite>();
	if (!tutorialGimmickGuideSprite_->Initialize(
		spriteCommon, "Resources/ui/tutorial/gimickGuide.png")) {
		return false;
	}
	tutorialGimmickGuideSprite_->SetPosition({ 28.0f, 545.0f });
	// Crop the isolated pixels on the source image's top edge so they do not
	// become a visible line when the small guide texture is enlarged.
	tutorialGimmickGuideSprite_->SetTextureRect(
		{ 0.0f, 3.0f }, { 170.0f, 13.0f });
	tutorialGimmickGuideSprite_->SetSize({ 340.0f, 26.0f });
	tutorialGimmickGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	tutorialGimmickGuideSprite_->Update();
	tutorialSkipSprite_ = std::make_unique<Sprite>();
	if (!tutorialSkipSprite_->Initialize(
		spriteCommon, "Resources/ui/tutorial/Skip.png")) {
		return false;
	}
	// Keep the skip controls to the left of the minimap in the upper-right area.
	tutorialSkipSprite_->SetPosition({ 1038.0f, 24.0f });
	tutorialSkipSprite_->SetSize({ 64.0f, 64.0f });
	tutorialSkipSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	tutorialSkipSprite_->Update();
	tutorialSkipEnterSprite_ = std::make_unique<Sprite>();
	if (!tutorialSkipEnterSprite_->Initialize(
		spriteCommon, "Resources/ui/tutorial/SkipEnter.png")) {
		return false;
	}
	tutorialSkipEnterSprite_->SetPosition({ 1022.0f, 88.0f });
	tutorialSkipEnterSprite_->SetSize({ 96.0f, 32.0f });
	tutorialSkipEnterSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	tutorialSkipEnterSprite_->Update();
	tutorialObstacleGuidePanel_ = std::make_unique<Sprite>();
	if (!tutorialObstacleGuidePanel_->Initialize(
		spriteCommon, "Resources/human/white.png")) {
		return false;
	}
	tutorialObstacleGuidePanel_->SetPosition({ 30.0f, 72.0f });
	tutorialObstacleGuidePanel_->SetSize({ 1220.0f, 570.0f });
	tutorialObstacleGuidePanel_->SetColor({ 0.015f, 0.03f, 0.055f, 0.94f });
	tutorialObstacleGuidePanel_->Update();

	const std::array<Vector4, 7> iconColors = {
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
	};
	const std::array<const char*, 7> iconTextures = {
		"Resources/magnet/wall/wall_preview.png",
		"Resources/magnet/chainsaw/chainsaw_preview.png",
		"Resources/magnet/pinball/pinball_bumper_preview.png",
		"Resources/magnet/furnace/magma_base.png",
		"Resources/ui/tutorial/anchor_icon.png",
		"Resources/magnet/shutter/timed_shutter_preview.png",
		"Resources/ui/tutorial/repulsion_icon.png"
	};
	const std::array<const char*, 7> names = {
		"壁", "チェーンソー", "ピンボールバンパー", "溶鉱炉",
		"磁石アンカー", "開閉シャッター", "反発磁場"
	};
	const std::array<const char*, 7> descriptions = {
		"プレイヤーと磁石の進行を止める",
		"接続中の磁石を切断する",
		"磁石を大きく跳ね返す",
		"触れた磁石を消滅させる",
		"磁石を引き寄せ固定する",
		"一定時間で開閉する",
		"磁石を外側へ押し出す"
	};
	if (!tutorialObstacleFont_.InitializeFromJson(spriteCommon,
		"Resources/ui/font/tutorial_obstacle_japanese_font.json")) {
		return false;
	}
	tutorialObstacleGuideTitle_.Initialize(spriteCommon, &tutorialObstacleFont_);
	tutorialObstacleGuideTitle_.SetText("ギミック説明");
	tutorialObstacleGuideTitle_.SetPosition({ 500.0f, 92.0f });
	tutorialObstacleGuideTitle_.SetScale(0.72f);
	tutorialObstacleGuideTitle_.SetColor(kUiAccentColor);
	tutorialObstacleGuideTitle_.Update();
	for (std::size_t index = 0; index < tutorialObstacleIcons_.size(); ++index) {
		const float rowY = 154.0f + 67.0f * static_cast<float>(index);
		tutorialObstacleIcons_[index] = std::make_unique<Sprite>();
		if (!tutorialObstacleIcons_[index]->Initialize(
			spriteCommon, iconTextures[index])) {
			return false;
		}
		tutorialObstacleIcons_[index]->SetPosition({ 72.0f, rowY - 6.0f });
		tutorialObstacleIcons_[index]->SetSize({ 74.0f, 42.0f });
		tutorialObstacleIcons_[index]->SetColor(iconColors[index]);
		tutorialObstacleIcons_[index]->Update();
		tutorialObstacleNames_[index].Initialize(spriteCommon, &tutorialObstacleFont_);
		tutorialObstacleNames_[index].SetText(names[index]);
		tutorialObstacleNames_[index].SetPosition({ 174.0f, rowY });
		tutorialObstacleNames_[index].SetScale(0.52f);
		tutorialObstacleNames_[index].SetColor(kUiTextColor);
		tutorialObstacleNames_[index].Update();
		tutorialObstacleDescriptions_[index].Initialize(spriteCommon, &tutorialObstacleFont_);
		tutorialObstacleDescriptions_[index].SetText(descriptions[index]);
		tutorialObstacleDescriptions_[index].SetPosition({ 520.0f, rowY });
		tutorialObstacleDescriptions_[index].SetScale(0.46f);
		tutorialObstacleDescriptions_[index].SetColor({ 0.62f, 0.84f, 0.92f, 1.0f });
		tutorialObstacleDescriptions_[index].Update();
	}
	tutorialMessageText_.Initialize(spriteCommon, &gameFlowFont_);
	tutorialMessageText_.SetPosition({ 28.0f, 470.0f });
	tutorialMessageText_.SetScale(1.05f);
	tutorialMessageText_.SetColor(kUiAccentColor);
	return true;
}

void MagnetPrototypeScene::UpdateTutorialUi()
{
	if (!tutorialMode_ || !tutorialUiReady_) { return; }
	char message[96]{};
	switch (tutorialPhase_) {
	case TutorialPhase::Movement:
		std::snprintf(message, sizeof(message), "MOVE WITH WASD  %d%%",
			static_cast<int>((std::min)(100.0f, tutorialMovementDistance_ * 40.0f)));
		break;
	case TutorialPhase::AttachMagnets:
		std::snprintf(message, sizeof(message), "%zu / 4",
			(std::min)(std::size_t{ 4 }, magnetChainSystem_.GetAttachedBallCount()));
		break;
	case TutorialPhase::ScoreGoal:
		std::snprintf(message, sizeof(message), "SHOOT A MAGNET INTO THE GOAL");
		break;
	case TutorialPhase::TryObstacles:
		std::snprintf(message, sizeof(message),
			"HOLD TAB: OBSTACLE GUIDE   SPACE: START GAME");
		break;
	}
	tutorialMessageText_.SetText(message);
	if (tutorialPhase_ == TutorialPhase::AttachMagnets) {
		tutorialMessageText_.SetPosition({ 104.0f, 590.0f });
		tutorialMessageText_.SetScale(1.6f);
	} else {
		tutorialMessageText_.SetPosition({ 28.0f, 470.0f });
		tutorialMessageText_.SetScale(1.05f);
	}
	tutorialMessageText_.Update();

	Input* input = framework_ ? framework_->GetInput() : nullptr;
	tutorialObstacleGuideVisible_ = tutorialPhase_ == TutorialPhase::TryObstacles &&
		input && input->PushKey(InputKey::Tab);
	const std::array<InputKey, 4> keys = {
		InputKey::W, InputKey::A, InputKey::S, InputKey::D
	};
	for (std::size_t index = 0; index < tutorialKeySprites_.size(); ++index) {
		const bool pressed = input && input->PushKey(keys[index]);
		tutorialKeySprites_[index]->SetColor(pressed
			? Vector4{ 1.0f, 0.78f, 0.18f, 0.98f }
			: Vector4{ 0.16f, 0.38f, 0.46f, 0.82f });
		tutorialKeySprites_[index]->Update();
	}
}

void MagnetPrototypeScene::DrawTutorialUi()
{
	if (!tutorialUiReady_) { return; }
	if (tutorialPhase_ == TutorialPhase::AttachMagnets) {
		tutorialAttractGuideSprite_->Draw();
		tutorialMessageText_.Draw();
		return;
	}
	if (tutorialPhase_ == TutorialPhase::ScoreGoal) {
		tutorialShootGuideSprite_->Draw();
		return;
	}
	if (tutorialPhase_ == TutorialPhase::TryObstacles) {
		if (tutorialObstacleGuideVisible_) {
			DrawTutorialObstacleGuide();
		} else {
			tutorialGimmickGuideSprite_->Draw();
			if (tutorialTransitionKeyObject_) {
				Object3dCommon* objectCommon = framework_->GetObject3dCommon();
				objectCommon->BeginObjectPass();
				tutorialTransitionKeyObject_->Draw();
				objectCommon->EndObjectPass();
				framework_->GetSpriteCommon()->PreDraw();
			}
		}
		return;
	}
	if (tutorialPhase_ != TutorialPhase::Movement) {
		tutorialMessageText_.Draw();
		return;
	}
	tutorialMoveGuideSprite_->Draw();
	for (auto& sprite : tutorialKeySprites_) { sprite->Draw(); }
	for (auto& sprite : tutorialKeyImageSprites_) { sprite->Draw(); }
}

void MagnetPrototypeScene::DrawTutorialSkipUi()
{
	if (tutorialSkipSprite_) { tutorialSkipSprite_->Draw(); }
	if (tutorialSkipEnterSprite_) { tutorialSkipEnterSprite_->Draw(); }
}

void MagnetPrototypeScene::DrawTutorialObstacleGuide()
{
	if (!tutorialObstacleGuidePanel_) { return; }
	tutorialObstacleGuidePanel_->Draw();
	for (auto& sprite : tutorialObstacleIcons_) { sprite->Draw(); }
	tutorialObstacleGuideTitle_.Draw();
	for (auto& text : tutorialObstacleNames_) { text.Draw(); }
	for (auto& text : tutorialObstacleDescriptions_) { text.Draw(); }
}

bool MagnetPrototypeScene::StartTutorialMagnetPhase()
{
	const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
		magnetChainSystem_.GetPlayerBody());
	if (player) { (void)magnetStageSystem_.SetPlayerPosition(player->position); }
	const std::array<Vector3, 6> balls = {
		Vector3{ -3.0f, 0.5f, -1.0f }, Vector3{ -1.6f, 0.5f, 2.0f },
		Vector3{ 0.0f, 0.5f, 3.2f }, Vector3{ 1.8f, 0.5f, 2.0f },
		Vector3{ 3.2f, 0.5f, -0.5f }, Vector3{ 0.0f, 0.5f, -3.0f }
	};
	for (const Vector3& position : balls) {
		if (!magnetStageSystem_.AddBall(position)) { return false; }
	}
	if (!magnetStageSystem_.AddBoxObject(
		magnet::MagnetStageObjectType::Goal, { 0.0f, 1.0f, 7.0f }, { 5.0f, 2.0f, 2.0f })) {
		return false;
	}
	const struct TutorialObstacle {
		Vector3 position;
		Vector3 size;
		magnet::MagnetObstacleKind kind;
	} obstacles[] = {
		{{-6.0f, 1.0f,  1.0f}, {1.6f, 2.0f, 1.6f}, magnet::MagnetObstacleKind::PinballBumper},
		{{ 6.0f, 1.0f,  1.0f}, {1.6f, 2.0f, 1.6f}, magnet::MagnetObstacleKind::MagneticAnchor},
		{{-5.0f, 1.0f, -5.0f}, {2.0f, 2.0f, 2.0f}, magnet::MagnetObstacleKind::Chainsaw},
		{{ 5.0f, 1.0f, -5.0f}, {2.0f, 2.0f, 2.0f}, magnet::MagnetObstacleKind::Furnace},
		{{ 0.0f, 1.0f, -6.5f}, {2.0f, 2.0f, 2.0f}, magnet::MagnetObstacleKind::RepulsionField},
		{{-3.0f, 1.0f,  5.0f}, {1.2f, 2.0f, 3.0f}, magnet::MagnetObstacleKind::TimedShutter},
		{{ 3.0f, 1.0f,  5.0f}, {1.2f, 2.0f, 3.0f}, magnet::MagnetObstacleKind::Solid},
	};
	for (const auto& obstacle : obstacles) {
		if (!magnetStageSystem_.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			obstacle.position, obstacle.size, obstacle.kind)) {
			return false;
		}
	}
	if (!magnetChainSystem_.ApplyStageLayout(magnetStageSystem_.GetStageData())) {
		return false;
	}
	magnetChainSystem_.SetActiveGoalCount(0);
	magnetChainSystem_.SetActiveObstacleCount(0);
	magneticImpactFeedbackSystem_.Reset();
	return true;
}

void MagnetPrototypeScene::SkipTutorialPhase()
{
	switch (tutorialPhase_) {
	case TutorialPhase::Movement:
		if (StartTutorialMagnetPhase()) {
			tutorialPhase_ = TutorialPhase::AttachMagnets;
			ballVisualsReady_ = UpdateBallVisuals(0.0f);
		}
		break;
	case TutorialPhase::AttachMagnets:
		tutorialPhase_ = TutorialPhase::ScoreGoal;
		tutorialScoreAtGoalStart_ = magnetChainSystem_.GetScore();
		magnetChainSystem_.SetActiveGoalCount(1);
		break;
	case TutorialPhase::ScoreGoal:
		tutorialPhase_ = TutorialPhase::TryObstacles;
		magnetChainSystem_.SetActiveObstacleCount(
			magnetStageSystem_.GetStageData().obstacleCount);
		break;
	case TutorialPhase::TryObstacles:
		rankingTransitionRequested_ = true;
		SceneManager::GetInstance()->ChangeScene("MAGNET_PROTOTYPE");
		break;
	}
	UpdateTutorialUi();
}

void MagnetPrototypeScene::UpdateTutorialProgress(float fixedDeltaTime)
{
	if (tutorialPhase_ == TutorialPhase::Movement) {
		if (HasMovementInput(pendingCommand_.moveDirection)) {
			tutorialMovementDistance_ += fixedDeltaTime;
		}
		if (tutorialMovementDistance_ >= 2.5f) {
			if (StartTutorialMagnetPhase()) {
				tutorialPhase_ = TutorialPhase::AttachMagnets;
				ballVisualsReady_ = UpdateBallVisuals(0.0f);
			}
		}
	} else if (tutorialPhase_ == TutorialPhase::AttachMagnets &&
		magnetChainSystem_.GetAttachedBallCount() >= 4) {
		tutorialPhase_ = TutorialPhase::ScoreGoal;
		tutorialScoreAtGoalStart_ = magnetChainSystem_.GetScore();
		magnetChainSystem_.SetActiveGoalCount(1);
	} else if (tutorialPhase_ == TutorialPhase::ScoreGoal &&
		magnetChainSystem_.GetScore() > tutorialScoreAtGoalStart_) {
		tutorialPhase_ = TutorialPhase::TryObstacles;
		magnetChainSystem_.SetActiveObstacleCount(
			magnetStageSystem_.GetStageData().obstacleCount);
	}
}

void MagnetPrototypeScene::CompleteTimedGame()
{
	if (rankingTransitionRequested_) { return; }
	rankingTransitionRequested_ = true;
	GameFlowState::GetInstance().SubmitScore(magnetChainSystem_.GetScore());
	SceneManager::GetInstance()->ChangeScene("RANKING");
}

bool MagnetPrototypeScene::InitializeGoalGuides()
{
	SpriteCommon* spriteCommon = framework_ ? framework_->GetSpriteCommon() : nullptr;
	if (!spriteCommon) { return false; }
	for (auto& guide : goalGuideSprites_) {
		for (auto& arm : guide) {
			arm = std::make_unique<Sprite>();
			if (!arm->Initialize(spriteCommon, "Resources/human/white.png")) {
				return false;
			}
			arm->SetSize({ kGoalGuideArmLength, kGoalGuideThickness });
			arm->SetColor({ 0.25f, 1.0f, 0.55f, 0.96f });
		}
	}
	return true;
}

void MagnetPrototypeScene::UpdateGoalGuides()
{
	goalGuideCount_ = 0;
	if (!goalGuidesReady_ || !prototypeReady_ || !camera_ ||
		editorMode_ != magnet::MagnetEditorMode::Play ||
		(tutorialMode_ && tutorialPhase_ < TutorialPhase::ScoreGoal)) {
		return;
	}

	const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
	const magnet::MagnetStageData& stageData = magnetStageSystem_.GetStageData();
	const Vector2 screenCenter = {
		kVirtualScreenSize.x * 0.5f,
		kVirtualScreenSize.y * 0.5f,
	};
	const Vector2 guideHalfExtents = {
		screenCenter.x - kGoalGuideScreenMargin,
		screenCenter.y - kGoalGuideScreenMargin,
	};

	for (std::size_t index = 0;
		index < stageData.goalCount && goalGuideCount_ < goalGuideSprites_.size(); ++index) {
		Vector3 position = index < magnetChainSystem_.GetGoalCount()
			? magnetChainSystem_.GetGoal(index).center
			: stageData.goals[index].position;
		position.y = stageData.goals[index].position.y;
		const float clipX = position.x * viewProjection.m[0][0] +
			position.y * viewProjection.m[1][0] +
			position.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float clipY = position.x * viewProjection.m[0][1] +
			position.y * viewProjection.m[1][1] +
			position.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float clipZ = position.x * viewProjection.m[0][2] +
			position.y * viewProjection.m[1][2] +
			position.z * viewProjection.m[2][2] + viewProjection.m[3][2];
		const float clipW = position.x * viewProjection.m[0][3] +
			position.y * viewProjection.m[1][3] +
			position.z * viewProjection.m[2][3] + viewProjection.m[3][3];
		const bool inFront = clipW > 0.0001f;
		const float inverseW = inFront ? 1.0f / clipW : 0.0f;
		const float ndcX = clipX * inverseW;
		const float ndcY = clipY * inverseW;
		const float ndcZ = clipZ * inverseW;
		const bool visible = inFront && ndcX >= -1.0f && ndcX <= 1.0f &&
			ndcY >= -1.0f && ndcY <= 1.0f && ndcZ >= 0.0f && ndcZ <= 1.0f;
		if (visible) { continue; }

		Vector2 direction = { clipX, -clipY };
		if (!inFront) {
			direction.x = -direction.x;
			direction.y = -direction.y;
		}
		const float directionLength = std::sqrt(
			direction.x * direction.x + direction.y * direction.y);
		if (directionLength < 0.0001f) {
			direction = { 0.0f, 1.0f };
		} else {
			direction.x /= directionLength;
			direction.y /= directionLength;
		}
		const float scaleX = std::abs(direction.x) > 0.0001f
			? guideHalfExtents.x / std::abs(direction.x) : 100000.0f;
		const float scaleY = std::abs(direction.y) > 0.0001f
			? guideHalfExtents.y / std::abs(direction.y) : 100000.0f;
		const float edgeScale = (std::min)(scaleX, scaleY);
		const Vector2 tip = {
			screenCenter.x + direction.x * edgeScale,
			screenCenter.y + direction.y * edgeScale,
		};
		const float directionAngle = std::atan2(direction.y, direction.x);
		auto& guide = goalGuideSprites_[goalGuideCount_++];
		guide[0]->SetPosition(tip);
		guide[0]->SetRotation(directionAngle + 3.14159265f - kGoalGuideHalfAngle);
		guide[1]->SetPosition(tip);
		guide[1]->SetRotation(directionAngle + 3.14159265f + kGoalGuideHalfAngle);
		guide[0]->Update();
		guide[1]->Update();
	}
}

void MagnetPrototypeScene::DrawGoalGuides()
{
	if (!goalGuidesReady_ || goalGuideCount_ == 0) { return; }
	framework_->GetSpriteCommon()->PreDraw();
	for (std::size_t index = 0; index < goalGuideCount_; ++index) {
		goalGuideSprites_[index][0]->Draw();
		goalGuideSprites_[index][1]->Draw();
	}
}

bool MagnetPrototypeScene::InitializeMinimap()
{
	SpriteCommon* spriteCommon = framework_ ? framework_->GetSpriteCommon() : nullptr;
	if (!spriteCommon) { return false; }
	const auto createSprite = [spriteCommon](const char* texture, const Vector2& size,
		const Vector4& color) -> std::unique_ptr<Sprite> {
		auto sprite = std::make_unique<Sprite>();
		if (!sprite->Initialize(spriteCommon, texture)) { return {}; }
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		sprite->SetSize(size);
		sprite->SetColor(color);
		return sprite;
	};
	minimapBorderSprite_ = createSprite("Resources/human/white.png",
		{ kMinimapBorderSize, kMinimapBorderSize }, { 0.10f, 0.64f, 0.78f, 0.90f });
	minimapBackgroundSprite_ = createSprite("Resources/human/white.png",
		{ kMinimapSize, kMinimapSize }, { 0.015f, 0.025f, 0.045f, 0.94f });
	minimapHorizontalGuideSprite_ = createSprite("Resources/human/white.png",
		{ kMinimapSize - kMinimapGuideInset * 2.0f, kMinimapGuideThickness },
		{ 0.22f, 0.62f, 0.70f, 0.30f });
	minimapVerticalGuideSprite_ = createSprite("Resources/human/white.png",
		{ kMinimapGuideThickness, kMinimapSize - kMinimapGuideInset * 2.0f },
		{ 0.22f, 0.62f, 0.70f, 0.30f });
	minimapPlayerSprite_ = createSprite("Resources/ui/minimap_player.png",
		{ kMinimapPlayerSize, kMinimapPlayerSize }, { 1.0f, 1.0f, 1.0f, 1.0f });
	if (!minimapBorderSprite_ || !minimapBackgroundSprite_ ||
		!minimapHorizontalGuideSprite_ || !minimapVerticalGuideSprite_ ||
		!minimapPlayerSprite_) {
		return false;
	}
	// Keep magnet markers as crisp red blocks so they cannot be confused with the
	// larger cyan-ring player icon.
	for (auto& marker : minimapMagnetSprites_) {
		marker = createSprite("Resources/human/white.png",
			{ kMinimapMarkerSize, kMinimapMarkerSize }, { 1.0f, 0.22f, 0.10f, 1.0f });
		if (!marker) { return false; }
	}
	return true;
}

void MagnetPrototypeScene::UpdateMinimap()
{
	minimapMagnetCount_ = 0;
	if (!minimapReady_ || !camera_ || !prototypeReady_) { return; }
	const Vector2 mapTopLeft = {
		kVirtualScreenSize.x - kMinimapMargin - kMinimapSize,
		kMinimapMargin,
	};
	const Vector2 center = {
		mapTopLeft.x + kMinimapSize * 0.5f,
		mapTopLeft.y + kMinimapSize * 0.5f,
	};
	// Sprite currently uses its position as the top-left corner. Place each element
	// explicitly from that convention so the frame stays inside the render target.
	const float borderInset = (kMinimapBorderSize - kMinimapSize) * 0.5f;
	minimapBorderSprite_->SetPosition({
		mapTopLeft.x - borderInset,
		mapTopLeft.y - borderInset,
	});
	minimapBackgroundSprite_->SetPosition(mapTopLeft);
	minimapHorizontalGuideSprite_->SetPosition({
		mapTopLeft.x + kMinimapGuideInset,
		center.y - kMinimapGuideThickness * 0.5f,
	});
	minimapVerticalGuideSprite_->SetPosition({
		center.x - kMinimapGuideThickness * 0.5f,
		mapTopLeft.y + kMinimapGuideInset,
	});
	minimapPlayerSprite_->SetPosition({
		center.x - kMinimapPlayerSize * 0.5f,
		center.y - kMinimapPlayerSize * 0.5f,
	});
	const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
		magnetChainSystem_.GetPlayerBody());
	if (!player || !player->active) { return; }

	const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
	const auto& stageBalls = magnetChainSystem_.GetStageBalls();
	const float usableRadius = kMinimapSize * 0.5f - kMinimapMarkerSize;
	for (std::size_t index = 0;
		index < magnetChainSystem_.GetStageBallCount() &&
		minimapMagnetCount_ < minimapMagnetSprites_.size(); ++index) {
		const physics::SphereBody* ball =
			magnetChainSystem_.GetPhysicsWorld().GetBody(stageBalls[index]);
		if (!ball || !ball->active) { continue; }
		const Vector3 ndc = MatrixMath::Transform(ball->position, viewProjection);
		const float clipW = ball->position.x * viewProjection.m[0][3] +
			ball->position.y * viewProjection.m[1][3] +
			ball->position.z * viewProjection.m[2][3] + viewProjection.m[3][3];
		const bool visible = clipW > 0.0f && ndc.x >= -1.0f && ndc.x <= 1.0f &&
			ndc.y >= -1.0f && ndc.y <= 1.0f && ndc.z >= 0.0f && ndc.z <= 1.0f;
		if (visible) { continue; }
		const Vector3 worldOffset = ball->position - player->position;
		float normalizedX = worldOffset.x / kMinimapWorldRadius;
		float normalizedZ = worldOffset.z / kMinimapWorldRadius;
		const float maximumComponent = (std::max)(std::abs(normalizedX), std::abs(normalizedZ));
		if (maximumComponent > 1.0f) {
			normalizedX /= maximumComponent;
			normalizedZ /= maximumComponent;
		}
		Sprite* marker = minimapMagnetSprites_[minimapMagnetCount_++].get();
		marker->SetPosition({
			center.x + normalizedX * usableRadius - kMinimapMarkerSize * 0.5f,
			center.y - normalizedZ * usableRadius - kMinimapMarkerSize * 0.5f,
		});
	}
	minimapBorderSprite_->Update();
	minimapBackgroundSprite_->Update();
	minimapHorizontalGuideSprite_->Update();
	minimapVerticalGuideSprite_->Update();
	minimapPlayerSprite_->Update();
	for (std::size_t index = 0; index < minimapMagnetCount_; ++index) {
		minimapMagnetSprites_[index]->Update();
	}
}

void MagnetPrototypeScene::DrawMinimap()
{
	if (!minimapReady_ || !prototypeReady_) { return; }
	framework_->GetSpriteCommon()->PreDraw();
	minimapBorderSprite_->Draw();
	minimapBackgroundSprite_->Draw();
	minimapHorizontalGuideSprite_->Draw();
	minimapVerticalGuideSprite_->Draw();
	minimapPlayerSprite_->Draw();
	for (std::size_t index = 0; index < minimapMagnetCount_; ++index) {
		minimapMagnetSprites_[index]->Draw();
	}
}

void MagnetPrototypeScene::ProcessStageEditorRequest(
	const magnet::MagnetPrototypeUiRequest& request)
{
	bool stageChanged = false;
	switch (request.stageAction) {
	case magnet::MagnetStageEditorAction::SetArenaRadius:
		stageChanged = magnetStageSystem_.SetArenaRadius(request.arenaRadius);
		break;
	case magnet::MagnetStageEditorAction::SetTimeLimit:
		stageChanged = magnetStageSystem_.SetTimeLimitSeconds(
			request.timeLimitSeconds);
		break;
	case magnet::MagnetStageEditorAction::GenerateBalanced:
		stageChanged = magnetStageSystem_.GenerateBalanced(request.generationSettings);
		break;
	case magnet::MagnetStageEditorAction::MovePlayer:
		stageChanged = magnetStageSystem_.SetPlayerPosition(
			request.editedObjectPosition);
		break;
	case magnet::MagnetStageEditorAction::AddBall:
		stageChanged = magnetStageSystem_.AddBall(
			CalculateEditorPlacementPosition(request.editedObjectPosition.y));
		break;
	case magnet::MagnetStageEditorAction::RemoveBall:
		stageChanged = magnetStageSystem_.RemoveBall(request.selectedBallId);
		break;
	case magnet::MagnetStageEditorAction::MoveBall:
		stageChanged = magnetStageSystem_.SetBallPosition(
			request.selectedBallId,
			request.editedObjectPosition);
		break;
	case magnet::MagnetStageEditorAction::AddGoal:
		stageChanged = magnetStageSystem_.AddBoxObject(
			magnet::MagnetStageObjectType::Goal,
			CalculateEditorPlacementPosition(request.editedObjectPosition.y),
			request.editedObjectSize);
		break;
	case magnet::MagnetStageEditorAction::AddObstacle:
		stageChanged = magnetStageSystem_.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			CalculateEditorPlacementPosition(request.editedObjectPosition.y),
			request.editedObjectSize,
			request.editedObstacleKind);
		break;
	case magnet::MagnetStageEditorAction::RemoveBoxObject:
		stageChanged = magnetStageSystem_.RemoveBoxObject(
			request.selectedObjectType,
			request.selectedObjectId);
		break;
	case magnet::MagnetStageEditorAction::MoveBoxObject:
		stageChanged = magnetStageSystem_.SetBoxObjectTransform(
			request.selectedObjectType,
			request.selectedObjectId,
			request.editedObjectPosition,
			request.editedObjectSize,
			request.editedObjectRotationYDegrees);
		break;
	case magnet::MagnetStageEditorAction::UpdateGoalScore:
		stageChanged = magnetStageSystem_.SetGoalScore(
			request.selectedObjectId,
			request.editedGoalScore);
		break;
	case magnet::MagnetStageEditorAction::UpdateObstacleKind:
		stageChanged = magnetStageSystem_.SetObstacleKind(
			request.selectedObjectId,
			request.editedObstacleKind);
		break;
	case magnet::MagnetStageEditorAction::UpdateTransferPairId:
		stageChanged = magnetStageSystem_.SetTransferPairId(
			request.selectedObjectId,
			request.editedTransferPairId);
		break;
	case magnet::MagnetStageEditorAction::UpdateAnchorAttractionRadius:
		stageChanged = magnetStageSystem_.SetAnchorAttractionRadius(
			request.selectedObjectId,
			request.editedAnchorAttractionRadius);
		break;
	case magnet::MagnetStageEditorAction::SaveNamed:
		if (magnetStageSystem_.SaveNamed(
			request.stageSaveName.data(),
			request.allowOverwrite)) {
			GameFlowState::GetInstance().SetActiveStageSaveName(
				request.stageSaveName.data());
		}
		break;
	case magnet::MagnetStageEditorAction::LoadNamed:
		stageChanged = magnetStageSystem_.LoadNamed(request.stageSaveName.data());
		if (stageChanged) {
			GameFlowState::GetInstance().SetActiveStageSaveName(
				request.stageSaveName.data());
		}
		break;
	case magnet::MagnetStageEditorAction::RefreshSaves:
		(void)magnetStageSystem_.RefreshSaveEntries();
		break;
	case magnet::MagnetStageEditorAction::None:
	default:
		break;
	}
	if (stageChanged &&
		!magnetChainSystem_.ApplyStageLayout(magnetStageSystem_.GetStageData())) {
		prototypeReady_ = false;
		Logger::Log("MagnetPrototypeScene: applying edited stage layout failed.");
		assert(false && "Applying edited magnet stage failed.");
	}
}

void MagnetPrototypeScene::HandleStageEditorKeyboardInput(float deltaTime)
{
	if (!prototypeReady_ || editorMode_ != magnet::MagnetEditorMode::StageEdit ||
		!framework_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f ||
		ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}
	Input* input = framework_->GetInput();
	if (!input) { return; }

	Vector3 moveDirection{};
	if (input->PushKey(InputKey::A)) { moveDirection.x -= 1.0f; }
	if (input->PushKey(InputKey::D)) { moveDirection.x += 1.0f; }
	if (input->PushKey(InputKey::W)) { moveDirection.z += 1.0f; }
	if (input->PushKey(InputKey::S)) { moveDirection.z -= 1.0f; }
	const float lengthSquared = moveDirection.x * moveDirection.x +
		moveDirection.z * moveDirection.z;
	if (lengthSquared > 1.0f) {
		moveDirection = moveDirection * (1.0f / std::sqrt(lengthSquared));
	}
	constexpr float kObjectMoveSpeed = 5.0f;
	constexpr float kRotationStepDegrees = 15.0f;
	const Vector3 movement = moveDirection * (kObjectMoveSpeed * deltaTime);
	const bool rotateRequested = input->TriggerKey(InputKey::R);
	if (lengthSquared <= 0.0f && !rotateRequested) { return; }

	bool stageChanged = false;
	switch (selectedObjectType_) {
	case magnet::MagnetStageObjectType::Player:
		if (lengthSquared > 0.0f) {
			stageChanged = magnetStageSystem_.SetPlayerPosition(
				magnetStageSystem_.GetStageData().playerPosition + movement);
		}
		break;
	case magnet::MagnetStageObjectType::MagnetBall: {
		const magnet::MagnetStageBallPlacement* ball =
			magnetStageSystem_.FindBall(selectedObjectId_);
		if (ball && lengthSquared > 0.0f) {
			stageChanged = magnetStageSystem_.SetBallPosition(
				selectedObjectId_, ball->position + movement);
		}
		break;
	}
	case magnet::MagnetStageObjectType::Goal:
	case magnet::MagnetStageObjectType::Obstacle: {
		const magnet::MagnetStageBoxPlacement* object =
			magnetStageSystem_.FindBoxObject(selectedObjectType_, selectedObjectId_);
		if (object) {
			stageChanged = magnetStageSystem_.SetBoxObjectTransform(
				selectedObjectType_, selectedObjectId_, object->position + movement,
				object->size,
				object->rotationYDegrees +
					(rotateRequested ? kRotationStepDegrees : 0.0f));
		}
		break;
	}
	case magnet::MagnetStageObjectType::None:
	default:
		break;
	}
	if (stageChanged &&
		!magnetChainSystem_.ApplyStageLayout(magnetStageSystem_.GetStageData())) {
		prototypeReady_ = false;
		Logger::Log("MagnetPrototypeScene: keyboard stage edit failed to apply.");
		assert(false && "Applying keyboard stage edit failed.");
	}
}

void MagnetPrototypeScene::SelectStageObjectAtNdc(const Vector2& clickNdc)
{
	if (!camera_ || !std::isfinite(clickNdc.x) || !std::isfinite(clickNdc.y)) {
		return;
	}
	const Matrix4x4 inverseViewProjection =
		MatrixMath::Inverse(camera_->GetViewProjectionMatrix());
	const Vector3 rayOrigin = MatrixMath::Transform(
		{ clickNdc.x, clickNdc.y, 0.0f }, inverseViewProjection);
	const Vector3 farPoint = MatrixMath::Transform(
		{ clickNdc.x, clickNdc.y, 1.0f }, inverseViewProjection);
	const Vector3 rayDirection = MatrixMath::Normalize(farPoint - rayOrigin);
	if (!IsFiniteVector3(rayOrigin) || !IsFiniteVector3(rayDirection)) { return; }

	magnet::MagnetStageObjectType bestType = magnet::MagnetStageObjectType::None;
	uint32_t bestId = 0;
	float nearestDistance = (std::numeric_limits<float>::max)();
	const auto acceptHit = [&](float distance,
		magnet::MagnetStageObjectType type, uint32_t id) {
		if (std::isfinite(distance) && distance >= 0.0f &&
			distance < nearestDistance) {
			nearestDistance = distance;
			bestType = type;
			bestId = id;
		}
	};
	const auto considerSphere = [&](const Vector3& center, float radius,
		magnet::MagnetStageObjectType type, uint32_t id) {
		const Vector3 offset = rayOrigin - center;
		const float projection = MatrixMath::Dot(offset, rayDirection);
		const float discriminant = projection * projection -
			(MatrixMath::Dot(offset, offset) - radius * radius);
		if (discriminant < 0.0f) { return; }
		const float root = std::sqrt(discriminant);
		const float nearDistance = -projection - root;
		const float farDistance = -projection + root;
		acceptHit(nearDistance >= 0.0f ? nearDistance : farDistance, type, id);
	};
	const auto considerBox = [&](const magnet::MagnetStageBoxPlacement& box,
		magnet::MagnetStageObjectType type) {
		const float radians = box.rotationYDegrees *
			0.01745329251994329577f;
		const float cosine = std::cos(radians);
		const float sine = std::sin(radians);
		const Vector3 offset = rayOrigin - box.position;
		const Vector3 localOrigin{
			offset.x * cosine - offset.z * sine,
			offset.y,
			offset.x * sine + offset.z * cosine,
		};
		const Vector3 localDirection{
			rayDirection.x * cosine - rayDirection.z * sine,
			rayDirection.y,
			rayDirection.x * sine + rayDirection.z * cosine,
		};
		const Vector3 half = box.size * 0.5f;
		float minimumDistance = 0.0f;
		float maximumDistance = nearestDistance;
		const auto intersectAxis = [&](float origin, float direction, float extent) {
			if (std::abs(direction) <= 1.0e-6f) {
				return origin >= -extent && origin <= extent;
			}
			float first = (-extent - origin) / direction;
			float second = (extent - origin) / direction;
			if (first > second) { std::swap(first, second); }
			minimumDistance = (std::max)(minimumDistance, first);
			maximumDistance = (std::min)(maximumDistance, second);
			return minimumDistance <= maximumDistance;
		};
		if (intersectAxis(localOrigin.x, localDirection.x, half.x) &&
			intersectAxis(localOrigin.y, localDirection.y, half.y) &&
			intersectAxis(localOrigin.z, localDirection.z, half.z)) {
			acceptHit(minimumDistance, type, box.id);
		}
	};

	const magnet::MagnetStageData& stage = magnetStageSystem_.GetStageData();
	considerSphere(stage.playerPosition, 0.75f,
		magnet::MagnetStageObjectType::Player, 0);
	for (std::size_t index = 0; index < stage.ballCount; ++index) {
		considerSphere(stage.balls[index].position, 0.5f,
			magnet::MagnetStageObjectType::MagnetBall, stage.balls[index].id);
	}
	for (std::size_t index = 0; index < stage.goalCount; ++index) {
		considerBox(stage.goals[index], magnet::MagnetStageObjectType::Goal);
	}
	for (std::size_t index = 0; index < stage.obstacleCount; ++index) {
		considerBox(stage.obstacles[index], magnet::MagnetStageObjectType::Obstacle);
	}

	selectedObjectType_ = bestType;
	selectedObjectId_ = bestId;
	prototypeWindow_.SetSelection(bestType, bestId);
}

Vector3 MagnetPrototypeScene::CalculateEditorPlacementPosition(float height) const noexcept
{
	Vector3 fallback = ResolveEditorFocusPosition();
	fallback.y = height;
	if (!camera_ || !std::isfinite(height)) { return fallback; }
	const Matrix4x4 inverseViewProjection =
		MatrixMath::Inverse(camera_->GetViewProjectionMatrix());
	const Vector3 nearPoint = MatrixMath::Transform(
		{ 0.0f, -0.25f, 0.0f }, inverseViewProjection);
	const Vector3 farPoint = MatrixMath::Transform(
		{ 0.0f, -0.25f, 1.0f }, inverseViewProjection);
	const Vector3 direction = MatrixMath::Normalize(farPoint - nearPoint);
	if (!IsFiniteVector3(direction) || std::abs(direction.y) <= 0.0001f) {
		return fallback;
	}
	const float distance = (height - nearPoint.y) / direction.y;
	if (!std::isfinite(distance) || distance < 0.0f) { return fallback; }
	Vector3 position = nearPoint + direction * distance;
	position.y = height;
	const float usableRadius = (std::max)(
		magnetStageSystem_.GetStageData().arenaRadius - 0.75f, 0.0f);
	const float radialSquared = position.x * position.x + position.z * position.z;
	if (radialSquared > usableRadius * usableRadius && radialSquared > 1.0e-8f) {
		const float scale = usableRadius / std::sqrt(radialSquared);
		position.x *= scale;
		position.z *= scale;
	}
	return IsFiniteVector3(position) ? position : fallback;
}

void MagnetPrototypeScene::ValidateEditorSelection() noexcept
{
	bool valid = selectedObjectType_ == magnet::MagnetStageObjectType::None ||
		selectedObjectType_ == magnet::MagnetStageObjectType::Player;
	if (selectedObjectType_ == magnet::MagnetStageObjectType::MagnetBall) {
		valid = magnetStageSystem_.FindBall(selectedObjectId_) != nullptr;
	} else if (selectedObjectType_ == magnet::MagnetStageObjectType::Goal ||
		selectedObjectType_ == magnet::MagnetStageObjectType::Obstacle) {
		valid = magnetStageSystem_.FindBoxObject(
			selectedObjectType_, selectedObjectId_) != nullptr;
	}
	if (!valid) {
		selectedObjectType_ = magnet::MagnetStageObjectType::None;
		selectedObjectId_ = 0;
		prototypeWindow_.SetSelection(selectedObjectType_, selectedObjectId_);
	}
}

void MagnetPrototypeScene::SetEditorMode(magnet::MagnetEditorMode mode)
{
	if (editorMode_ == mode || !prototypeReady_) {
		return;
	}
	if (!magnetChainSystem_.ApplyStageLayout(magnetStageSystem_.GetStageData())) {
		prototypeReady_ = false;
		Logger::Log("MagnetPrototypeScene: applying stage for editor mode failed.");
		assert(false && "Applying magnet stage for editor mode failed.");
		return;
	}
	editorMode_ = mode;
	if (editorMode_ == magnet::MagnetEditorMode::StageEdit) {
		magnetEditorCameraSystem_.Reset();
	}
	pendingCommand_ = {};
	resetRequested_ = false;
	releaseOverviewActive_ = false;
}

Vector3 MagnetPrototypeScene::ResolveEditorFocusPosition() const noexcept
{
	switch (selectedObjectType_) {
	case magnet::MagnetStageObjectType::MagnetBall: {
		const magnet::MagnetStageBallPlacement* ball =
			magnetStageSystem_.FindBall(selectedObjectId_);
		return ball && IsFiniteVector3(ball->position) ? ball->position : Vector3{};
	}
	case magnet::MagnetStageObjectType::Goal:
	case magnet::MagnetStageObjectType::Obstacle: {
		const magnet::MagnetStageBoxPlacement* object =
			magnetStageSystem_.FindBoxObject(selectedObjectType_, selectedObjectId_);
		return object && IsFiniteVector3(object->position) ? object->position : Vector3{};
	}
	case magnet::MagnetStageObjectType::Player: {
		const Vector3 playerPosition = magnetStageSystem_.GetStageData().playerPosition;
		return IsFiniteVector3(playerPosition) ? playerPosition : Vector3{};
	}
	case magnet::MagnetStageObjectType::None:
	default:
		break;
	}
	const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
		magnetChainSystem_.GetPlayerBody());
	return player && IsFiniteVector3(player->position) ? player->position : Vector3{};
}

Vector3 MagnetPrototypeScene::CalculatePlayCameraPosition() noexcept
{
	const physics::PhysicsWorld& world = magnetChainSystem_.GetPhysicsWorld();
	const physics::SphereBody* player = world.GetBody(magnetChainSystem_.GetPlayerBody());
	if (!player || !IsFiniteVector3(player->position)) {
		releaseOverviewActive_ = false;
		return camera_ ? camera_->GetTranslate() : Vector3{ 0.0f, 11.0f, -16.0f };
	}
	if (!releaseOverviewActive_) {
		const float cameraScale = std::clamp(
			magnetChainSystem_.GetArenaRadius() / 10.0f, 0.65f, 2.5f);
		return { player->position.x, 11.0f * cameraScale,
			player->position.z - 16.0f * cameraScale };
	}

	float minimumX = player->position.x;
	float maximumX = player->position.x;
	float minimumZ = player->position.z;
	float maximumZ = player->position.z;
	std::size_t releasedBodyCount = 0;
	const auto& balls = magnetChainSystem_.GetStageBalls();
	const auto& states = magnetChainSystem_.GetStageBallStates();
	for (std::size_t index = 0; index < magnetChainSystem_.GetStageBallCount(); ++index) {
		if (states[index] != magnet::MagnetChainSystem::StageBallState::Released) {
			continue;
		}
		const physics::SphereBody* body = world.GetBody(balls[index]);
		if (!body || !body->active || !IsFiniteVector3(body->position)) {
			continue;
		}
		minimumX = (std::min)(minimumX, body->position.x);
		maximumX = (std::max)(maximumX, body->position.x);
		minimumZ = (std::min)(minimumZ, body->position.z);
		maximumZ = (std::max)(maximumZ, body->position.z);
		++releasedBodyCount;
	}
	if (releasedBodyCount == 0) {
		releaseOverviewActive_ = false;
		return { player->position.x, 11.0f, player->position.z - 16.0f };
	}

	const float centerX = (minimumX + maximumX) * 0.5f;
	const float centerZ = (minimumZ + maximumZ) * 0.5f;
	const float span = (std::max)(maximumX - minimumX, maximumZ - minimumZ);
	const float boundedSpan = (std::clamp)(span, 0.0f, 40.0f);
	return {
		centerX,
		11.0f + boundedSpan * 0.35f,
		centerZ - (16.0f + boundedSpan * 0.75f),
	};
}

void MagnetPrototypeScene::DrawStageObjects() const
{
	const magnet::MagnetStageData& stageData = magnetStageSystem_.GetStageData();
	const std::size_t visibleGoalCount =
		(tutorialMode_ && tutorialPhase_ < TutorialPhase::ScoreGoal)
		? 0 : stageData.goalCount;
	const std::size_t visibleObstacleCount =
		(tutorialMode_ && tutorialPhase_ < TutorialPhase::TryObstacles)
		? 0 : stageData.obstacleCount;
	for (std::size_t index = 0; index < visibleGoalCount; ++index) {
		if (!stageStructureVisualsReady_) {
			Vector3 goalPosition = index < magnetChainSystem_.GetGoalCount()
				? magnetChainSystem_.GetGoal(index).center
				: stageData.goals[index].position;
			goalPosition.y = stageData.goals[index].position.y;
			DrawWireBox(
				goalPosition,
				stageData.goals[index].size,
				stageData.goals[index].rotationYDegrees,
				kGoalColor);
		}
	}
	for (std::size_t index = 0; index < visibleObstacleCount; ++index) {
		const magnet::MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		Vector3 runtimePosition = obstacle.position;
		float shutterOpenRatio = 0.0f;
		if (obstacle.obstacleKind == magnet::MagnetObstacleKind::TimedShutter) {
			shutterOpenRatio = magnetChainSystem_.GetTimedShutterOpenRatio(index);
			runtimePosition.y +=
				magnetChainSystem_.GetTimedShutterVerticalOffset(index, obstacle);
		}
		const bool shutterClosed = shutterOpenRatio < 0.5f;
		const Vector4 color = GetObstacleColor(
			obstacle.obstacleKind, shutterClosed);
		const bool hasGimmickVisual = magnetGimmickVisualsReady_ &&
			magnet::MagnetGimmickVisualSystem::Supports(obstacle.obstacleKind);
		const bool hasStructureVisual = stageStructureVisualsReady_ &&
			magnet::MagnetStageStructureVisualSystem::SupportsObstacle(
				obstacle.obstacleKind);
		if ((obstacle.obstacleKind != magnet::MagnetObstacleKind::Furnace ||
			!furnaceVisualsReady_) && !hasGimmickVisual && !hasStructureVisual) {
			DrawWireBox(
				runtimePosition,
				obstacle.size,
				obstacle.rotationYDegrees,
				color);
		}
		if ((obstacle.obstacleKind == magnet::MagnetObstacleKind::PinballBumper ||
			obstacle.obstacleKind == magnet::MagnetObstacleKind::MagneticAnchor) &&
			!magnetGimmickVisualsReady_) {
			LineDrawer::GetInstance()->DrawWireSphere(
				runtimePosition,
				(std::max)(obstacle.size.x, obstacle.size.z) * 0.5f,
				color,
				24);
		}
		if (obstacle.obstacleKind == magnet::MagnetObstacleKind::MagneticAnchor &&
			!magnetGimmickVisualsReady_) {
			const float attractionRadius =
				magnetChainSystem_.GetAnchorAttractionRadius(obstacle);
			for (int segment = 0; segment < kAnchorFieldSegments; ++segment) {
				const float firstAngle = 6.28318530717958647692f *
					static_cast<float>(segment) /
					static_cast<float>(kAnchorFieldSegments);
				const float secondAngle = 6.28318530717958647692f *
					static_cast<float>(segment + 1) /
					static_cast<float>(kAnchorFieldSegments);
				LineDrawer::GetInstance()->DrawLine(
					{
						obstacle.position.x + std::cos(firstAngle) * attractionRadius,
						0.03f,
						obstacle.position.z + std::sin(firstAngle) * attractionRadius,
					},
					{
						obstacle.position.x + std::cos(secondAngle) * attractionRadius,
						0.03f,
						obstacle.position.z + std::sin(secondAngle) * attractionRadius,
					},
					kAnchorFieldColor);
			}
			const physics::BodyHandle anchoredBody =
				magnetChainSystem_.GetAnchoredBody(index);
			const physics::SphereBody* body =
				magnetChainSystem_.GetPhysicsWorld().GetBody(anchoredBody);
			if (body && body->active) {
				LineDrawer::GetInstance()->DrawLine(
					obstacle.position, body->position, kAnchorColor);
			}
		}
		if (obstacle.obstacleKind == magnet::MagnetObstacleKind::RepulsionField &&
			!magnetGimmickVisualsReady_) {
			const float radius =
				magnetChainSystem_.GetRepulsionFieldRadius(obstacle);
			const float fieldY = obstacle.position.y - obstacle.size.y * 0.5f + 0.03f;
			for (int segment = 0; segment < kRepulsionFieldSegments; ++segment) {
				const float firstAngle = 6.28318530717958647692f *
					static_cast<float>(segment) /
					static_cast<float>(kRepulsionFieldSegments);
				const float secondAngle = 6.28318530717958647692f *
					static_cast<float>(segment + 1) /
					static_cast<float>(kRepulsionFieldSegments);
				LineDrawer::GetInstance()->DrawLine(
					{
						obstacle.position.x + std::cos(firstAngle) * radius,
						fieldY,
						obstacle.position.z + std::sin(firstAngle) * radius,
					},
					{
						obstacle.position.x + std::cos(secondAngle) * radius,
						fieldY,
						obstacle.position.z + std::sin(secondAngle) * radius,
					},
					kRepulsionRangeColor);
			}
			for (int arrow = 0; arrow < kRepulsionArrowCount; ++arrow) {
				const float angle = 6.28318530717958647692f *
					static_cast<float>(arrow) /
					static_cast<float>(kRepulsionArrowCount);
				const Vector3 direction{ std::cos(angle), 0.0f, std::sin(angle) };
				const Vector3 tangent{ -direction.z, 0.0f, direction.x };
				const Vector3 center{
					obstacle.position.x,
					fieldY,
					obstacle.position.z,
				};
				const Vector3 start = center + direction * (radius * 0.30f);
				const Vector3 end = center + direction * (radius * 0.88f);
				const Vector3 arrowBase = end - direction * (radius * 0.12f);
				const Vector3 arrowWidth = tangent * (radius * 0.055f);
				LineDrawer::GetInstance()->DrawLine(
					start, end, kRepulsionFieldColor);
				LineDrawer::GetInstance()->DrawLine(
					arrowBase + arrowWidth, end, kRepulsionFieldColor);
				LineDrawer::GetInstance()->DrawLine(
					arrowBase - arrowWidth, end, kRepulsionFieldColor);
			}
		}
	}
}

void MagnetPrototypeScene::DrawSelectionHighlight() const
{
	if (editorMode_ != magnet::MagnetEditorMode::StageEdit) {
		return;
	}
	if (selectedObjectType_ == magnet::MagnetStageObjectType::Player) {
		const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
			magnetChainSystem_.GetPlayerBody());
		if (player && player->active) {
			LineDrawer::GetInstance()->DrawWireSphere(
				player->position,
				player->radius + kSelectionSpherePadding,
				kSelectionColor,
				24);
		}
		return;
	}
	if (selectedObjectType_ == magnet::MagnetStageObjectType::MagnetBall) {
		const auto& ids = magnetChainSystem_.GetStageBallIds();
		const auto& balls = magnetChainSystem_.GetStageBalls();
		for (std::size_t index = 0; index < magnetChainSystem_.GetStageBallCount(); ++index) {
			if (ids[index] != selectedObjectId_) {
				continue;
			}
			const physics::SphereBody* body =
				magnetChainSystem_.GetPhysicsWorld().GetBody(balls[index]);
			if (body && body->active) {
				LineDrawer::GetInstance()->DrawWireSphere(
					body->position,
					body->radius + kSelectionSpherePadding,
					kSelectionColor,
					24);
			}
			return;
		}
		return;
	}
	if (selectedObjectType_ != magnet::MagnetStageObjectType::Goal &&
		selectedObjectType_ != magnet::MagnetStageObjectType::Obstacle) {
		return;
	}
	const magnet::MagnetStageBoxPlacement* object =
		magnetStageSystem_.FindBoxObject(selectedObjectType_, selectedObjectId_);
	if (object) {
		const Vector3 padding{
			kSelectionBoxPadding,
			kSelectionBoxPadding,
			kSelectionBoxPadding,
		};
		DrawWireBox(
			object->position,
			object->size + padding,
			object->rotationYDegrees,
			kSelectionColor);
	}
}

void MagnetPrototypeScene::DrawWireBox(
	const Vector3& center,
	const Vector3& size,
	float rotationYDegrees,
	const Vector4& color) const
{
	if (!IsFiniteVector3(center) || !IsFiniteVector3(size) ||
		!std::isfinite(rotationYDegrees) ||
		size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
		return;
	}
	const Vector3 half = size * 0.5f;
	const float rotationYRadians = rotationYDegrees *
		3.14159265358979323846f / 180.0f;
	const float cosine = std::cos(rotationYRadians);
	const float sine = std::sin(rotationYRadians);
	const auto toWorld = [&](const Vector3& local) noexcept {
		return Vector3{
			center.x + local.x * cosine + local.z * sine,
			center.y + local.y,
			center.z - local.x * sine + local.z * cosine,
		};
	};
	const Vector3 corners[8] = {
		toWorld({ -half.x, -half.y, -half.z }),
		toWorld({  half.x, -half.y, -half.z }),
		toWorld({  half.x,  half.y, -half.z }),
		toWorld({ -half.x,  half.y, -half.z }),
		toWorld({ -half.x, -half.y,  half.z }),
		toWorld({  half.x, -half.y,  half.z }),
		toWorld({  half.x,  half.y,  half.z }),
		toWorld({ -half.x,  half.y,  half.z }),
	};
	constexpr std::size_t kEdges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};
	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	for (const auto& edge : kEdges) {
		lineDrawer->DrawLine(corners[edge[0]], corners[edge[1]], color);
	}
}

bool MagnetPrototypeScene::InitializeBallVisuals()
{
	playerVisual_.reset();
	for (auto& visual : stageBallVisuals_) { visual.reset(); }
	if (!framework_ || !camera_ || !framework_->GetModelManager() ||
		!framework_->GetObject3dCommon()) {
		return false;
	}
	if (!IsRegularFileNoThrow(
			std::filesystem::path("Resources") / kPlayerModelPath) ||
		!IsRegularFileNoThrow(
			std::filesystem::path("Resources") / kSmallBallModelPath)) {
		return false;
	}

	ModelManager* modelManager = framework_->GetModelManager();
	modelManager->LoadModel(kPlayerModelPath);
	modelManager->LoadModel(kSmallBallModelPath);
	Model* playerModel = modelManager->GetModel(kPlayerModelPath);
	Model* smallBallModel = modelManager->GetModel(kSmallBallModelPath);
	if (!playerModel || !smallBallModel) {
		return false;
	}

	const Texture2DHandle whiteTexture =
		TextureManager::GetInstance()->LoadTexture2D("Resources/human/white.png");
	const auto createVisual = [&](Model* model, const Vector4& color) {
		auto visual = std::make_unique<Object3d>();
		visual->Initialize(framework_->GetObject3dCommon());
		visual->SetModel(model);
		visual->SetTexture(whiteTexture);
		visual->SetColor(color);
		visual->SetEnableLighting(true);
		visual->SetShininess(56.0f);
		return visual;
	};

	playerVisual_ = createVisual(playerModel, kPlayerColor);
	for (auto& visual : stageBallVisuals_) {
		visual = createVisual(smallBallModel, kAvailableBallColor);
	}
	return true;
}

bool MagnetPrototypeScene::UpdateBallVisuals(float deltaTime) noexcept
{
	if (!framework_ || !camera_ || !playerVisual_ ||
		magnetChainSystem_.GetStageBallCount() > stageBallVisuals_.size()) {
		return false;
	}
	const physics::PhysicsWorld& physicsWorld = magnetChainSystem_.GetPhysicsWorld();
	const auto updateVisual = [&](
		Object3d& visual,
		physics::BodyHandle handle,
		const Vector4& color) noexcept {
		const physics::SphereBody* body = physicsWorld.GetBody(handle);
		if (!body || !IsFiniteVector3(body->position) ||
			!IsFiniteVector3(body->angularVelocity)) {
			return false;
		}
		visual.SetPosition(body->position);
		if (!std::isfinite(body->radius) || body->radius <= 0.0f ||
			!visual.SetScale({ body->radius, body->radius, body->radius }) ||
			!visual.SetRotationQuaternion(body->orientation)) {
			return false;
		}
		visual.SetColor(color);
		visual.Update(camera_.get(), deltaTime);
		return true;
	};

	if (!updateVisual(
		*playerVisual_, magnetChainSystem_.GetPlayerBody(), kPlayerColor)) {
		return false;
	}
	const auto& stageBalls = magnetChainSystem_.GetStageBalls();
	const auto& states = magnetChainSystem_.GetStageBallStates();
	for (std::size_t index = 0;
		index < magnetChainSystem_.GetStageBallCount();
		++index) {
		if (!stageBallVisuals_[index] ||
			!updateVisual(
				*stageBallVisuals_[index],
				stageBalls[index],
				GetStageBallColor(states[index]))) {
			return false;
		}
	}
	return true;
}

void MagnetPrototypeScene::DrawBallVisuals() const
{
	if (!ballVisualsReady_ || !framework_ || !playerVisual_) {
		return;
	}
	Object3dCommon* object3dCommon = framework_->GetObject3dCommon();
	if (!object3dCommon) {
		return;
	}

	object3dCommon->BeginObjectPass();
	const physics::PhysicsWorld& physicsWorld = magnetChainSystem_.GetPhysicsWorld();
	const physics::SphereBody* player =
		physicsWorld.GetBody(magnetChainSystem_.GetPlayerBody());
	if (player && player->active) {
		playerVisual_->Draw();
	}
	const auto& stageBalls = magnetChainSystem_.GetStageBalls();
	const auto& states = magnetChainSystem_.GetStageBallStates();
	const std::size_t count = (std::min)(
		magnetChainSystem_.GetStageBallCount(), stageBallVisuals_.size());
	for (std::size_t index = 0; index < count; ++index) {
		const physics::SphereBody* body = physicsWorld.GetBody(stageBalls[index]);
		if (states[index] != magnet::MagnetChainSystem::StageBallState::Inactive &&
			body && body->active && stageBallVisuals_[index]) {
			stageBallVisuals_[index]->Draw();
		}
	}
	object3dCommon->EndObjectPass();
}

void MagnetPrototypeScene::DrawBody(physics::BodyHandle handle, const Vector4& color) const
{
	const physics::SphereBody* body = magnetChainSystem_.GetPhysicsWorld().GetBody(handle);
	if (body && body->active) {
		LineDrawer* lineDrawer = LineDrawer::GetInstance();
		if (!ballVisualsReady_) {
			lineDrawer->DrawWireSphere(body->position, body->radius, color, 18);
		}
		const Vector3 markerDirection = RotateByQuaternion(
			{ 0.0f, body->radius * 0.88f, 0.0f },
			body->orientation);
		lineDrawer->DrawLine(
			body->position - markerDirection,
			body->position + markerDirection,
			color);
	}
}

void MagnetPrototypeScene::DrawVelocity(physics::BodyHandle handle) const
{
	const physics::SphereBody* body = magnetChainSystem_.GetPhysicsWorld().GetBody(handle);
	if (body && body->active && showVelocity_) {
		LineDrawer::GetInstance()->DrawLine(
			body->position,
			body->position + body->linearVelocity * kVelocityDisplayScale,
			kVelocityColor);
	}
}
