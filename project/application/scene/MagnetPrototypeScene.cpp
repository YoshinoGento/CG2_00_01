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
#include <system_error>

namespace {

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
constexpr float kGameDurationSeconds = 60.0f;
constexpr int kPauseMenuItemCount = 4;
constexpr Vector4 kUiTextColor = { 0.88f, 0.94f, 0.98f, 1.0f };
constexpr Vector4 kUiAccentColor = { 1.0f, 0.82f, 0.24f, 1.0f };
constexpr float kComicTextMinimumImpactSpeed = 6.0f;

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
	skybox_ = std::make_unique<Skybox>();
	skybox_->InitializeGradient(
		framework_->GetDxCommon(),
		{ 0.22f, 0.38f, 0.62f, 1.0f },
		{ 0.70f, 0.30f, 0.34f, 1.0f });
	skybox_->Update(camera_.get());
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
	GameFlowState::GetInstance().EnsureBgm(framework_->GetAudio());

	prototypeReady_ = magnetStageSystem_.Initialize() &&
		magnetChainSystem_.Initialize(magnetStageSystem_.GetStageData());
	ballVisualsReady_ = prototypeReady_ && InitializeBallVisuals();
	if (ballVisualsReady_) {
		ballVisualsReady_ = UpdateBallVisuals(0.0f);
	}
	if (!ballVisualsReady_) {
		Logger::Log(
			"MagnetPrototypeScene: Player/SmallBall model initialization failed; using wire fallback.");
	}
	magneticImpactFeedbackSystem_.Reset();
	comicTextEffects_ = std::make_unique<ComicTextEffectSystem>();
	comicTextEffects_->Initialize(framework_->GetSpriteCommon());
	ComicTextEffectSystem::LoadPreset("HeavyImpact", heavyImpactPreset_);
	if (!prototypeReady_) {
		Logger::Log("MagnetPrototypeScene: MagnetChainSystem initialization failed.");
		assert(false && "MagnetChainSystem initialization failed.");
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
#ifdef USE_IMGUI
	particleEffectEditor_ = std::make_unique<ParticleEffectEditor>();
#endif
	RefreshGameFlowUi();
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
	comicTextEffects_.reset();
	playerVisual_.reset();
	for (auto& visual : stageBallVisuals_) { visual.reset(); }
	ballVisualsReady_ = false;
	volumeLabelObject_.reset();
	backTitleLabelObject_.reset();
	restartLabelObject_.reset();
	resumeLabelObject_.reset();
	pauseTitleObject_.reset();
	pauseLabelCamera_.reset();
	pauseOverlaySprite_.reset();
	gameFlowUiReady_ = false;
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
	gameElapsedSeconds_ += fixedDeltaTime;
	if (gameElapsedSeconds_ >= kGameDurationSeconds) {
		CompleteTimedGame();
		pendingCommand_ = {};
		return;
	}
	if (resetRequested_) {
		prototypeReady_ = magnetChainSystem_.Reset();
		magneticImpactFeedbackSystem_.Reset();
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
	magnetChainSystem_.SetPlayerCommand(pendingCommand_);
	prototypeReady_ = magnetChainSystem_.FixedUpdate(fixedDeltaTime);
	if (prototypeReady_) {
		const auto& impactEvents = magnetChainSystem_.GetMagneticImpactEvents();
		const std::size_t impactCount = magnetChainSystem_.GetMagneticImpactEventCount();
		magneticImpactFeedbackSystem_.AddImpacts(
			impactEvents, impactCount);
		if (comicTextEffects_) {
			for (std::size_t index = 0; index < impactCount; ++index) {
				if (impactEvents[index].relativeSpeed >= kComicTextMinimumImpactSpeed) {
					comicTextEffects_->Play(heavyImpactPreset_, impactEvents[index].position);
				}
			}
		}
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
}

void MagnetPrototypeScene::Update()
{
	const FrameClock* frameClock = framework_ ? framework_->GetFrameClock() : nullptr;
	const float frameDeltaSeconds = frameClock
		? frameClock->GetFrameDeltaSeconds()
		: FrameClock::kDefaultFixedDeltaSeconds;
	if (camera_) {
		Vector3 desiredPosition = camera_->GetTranslate();
		bool shouldMoveCamera = false;
		if (prototypeReady_ && editorMode_ == magnet::MagnetEditorMode::StageEdit) {
			const Vector3 focus = ResolveEditorFocusPosition();
			desiredPosition = { focus.x, focus.y + 9.0f, focus.z - 13.0f };
			shouldMoveCamera = IsFiniteVector3(desiredPosition);
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
	}
	if (ballVisualsReady_ && !UpdateBallVisuals(frameDeltaSeconds)) {
		Logger::Log(
			"MagnetPrototypeScene: ball visual update failed; using wire fallback.");
		ballVisualsReady_ = false;
	}
	if (framework_ && framework_->GetParticleManager() && camera_) {
		framework_->GetParticleManager()->Update(camera_.get(), frameDeltaSeconds);
	}
	if (comicTextEffects_ && camera_) {
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
	if (editorMode_ == magnet::MagnetEditorMode::Play) {
		resetRequested_ = resetRequested_ || request.reset;
		pendingCommand_.emergencyStop =
			pendingCommand_.emergencyStop || request.emergencyStop;
		pendingCommand_.releaseChains =
			pendingCommand_.releaseChains || request.releaseChains;
	}
	ProcessStageEditorRequest(request);
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
	DrawStageObjects();
	DrawSelectionHighlight();
	DrawBallVisuals();
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
	}

	const auto initializeText = [spriteCommon, this](SpriteText& text) {
		text.Initialize(spriteCommon, &gameFlowFont_);
		text.SetCharacterSpacing(-6.0f);
	};
	initializeText(timerText_);
	initializeText(pauseTitleText_);
	for (SpriteText& text : pauseMenuTexts_) { initializeText(text); }
	initializeText(pauseHelpText_);

	timerText_.SetPosition({ 565.0f, 18.0f });
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
	pauseHelpText_.SetText("DPAD OR STICK SELECT   B OK   MENU RESUME");
	pauseHelpText_.SetPosition({ 350.0f, 565.0f });
	pauseHelpText_.SetScale(0.62f);
	pauseHelpText_.SetColor({ 0.55f, 0.68f, 0.76f, 1.0f });
	pauseHelpText_.Update();
	return true;
}

void MagnetPrototypeScene::HandlePauseMenuInput(Input& input)
{
	const Vector2 stick = input.GetLeftStick();
	const bool stickUp = stick.y > 0.5f;
	const bool stickDown = stick.y < -0.5f;
	const bool stickLeft = stick.x < -0.5f;
	const bool stickRight = stick.x > 0.5f;
	const bool moveUp = input.TriggerKey(InputKey::ArrowUp) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadUp) ||
		(stickUp && !menuStickUpWasPressed_);
	const bool moveDown = input.TriggerKey(InputKey::ArrowDown) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadDown) ||
		(stickDown && !menuStickDownWasPressed_);
	const bool moveLeft = input.TriggerKey(InputKey::ArrowLeft) ||
		input.TriggerGamepadButton(InputGamepadButton::DPadLeft) ||
		(stickLeft && !menuStickLeftWasPressed_);
	const bool moveRight = input.TriggerKey(InputKey::ArrowRight) ||
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
	}
	if (input.TriggerKey(InputKey::Enter) ||
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
		(std::max)(0.0f, kGameDurationSeconds - gameElapsedSeconds_)));
	std::snprintf(timerBuffer, sizeof(timerBuffer), "TIME %02d", remainingSeconds);
	timerText_.SetText(timerBuffer);
	timerText_.Update();
	if (!paused_) { return; }

	const int volumePercent = static_cast<int>(std::lround(
		GameFlowState::GetInstance().GetBgmVolume() * 100.0f));
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
}

void MagnetPrototypeScene::DrawGameFlowUi()
{
	if (!gameFlowUiReady_) { return; }
	framework_->GetSpriteCommon()->PreDraw();
	timerText_.Draw();
	if (!paused_) { return; }
	pauseOverlaySprite_->Draw();
	pauseTitleText_.Draw();
	if (pauseTitleObject_ || resumeLabelObject_ || restartLabelObject_ ||
		backTitleLabelObject_ || volumeLabelObject_) {
		Object3dCommon* objectCommon = framework_->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		if (pauseTitleObject_) { pauseTitleObject_->Draw(); }
		if (resumeLabelObject_) { resumeLabelObject_->Draw(); }
		if (restartLabelObject_) { restartLabelObject_->Draw(); }
		if (backTitleLabelObject_) { backTitleLabelObject_->Draw(); }
		if (volumeLabelObject_) { volumeLabelObject_->Draw(); }
		objectCommon->EndObjectPass();
		framework_->GetSpriteCommon()->PreDraw();
	}
	for (SpriteText& text : pauseMenuTexts_) { text.Draw(); }
	pauseHelpText_.Draw();
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
		editorMode_ != magnet::MagnetEditorMode::Play) {
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
		const Vector3& position = stageData.goals[index].position;
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
	case magnet::MagnetStageEditorAction::GenerateBalanced:
		stageChanged = magnetStageSystem_.GenerateBalanced(request.generationSettings);
		break;
	case magnet::MagnetStageEditorAction::MovePlayer:
		stageChanged = magnetStageSystem_.SetPlayerPosition(
			request.editedObjectPosition);
		break;
	case magnet::MagnetStageEditorAction::AddBall:
		stageChanged = magnetStageSystem_.AddBall(request.editedObjectPosition);
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
			request.editedObjectPosition,
			request.editedObjectSize);
		break;
	case magnet::MagnetStageEditorAction::AddObstacle:
		stageChanged = magnetStageSystem_.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			request.editedObjectPosition,
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
			request.editedObjectSize);
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
	case magnet::MagnetStageEditorAction::SaveNamed:
		(void)magnetStageSystem_.SaveNamed(
			request.stageSaveName.data(),
			request.allowOverwrite);
		break;
	case magnet::MagnetStageEditorAction::LoadNamed:
		stageChanged = magnetStageSystem_.LoadNamed(request.stageSaveName.data());
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
	for (std::size_t index = 0; index < stageData.goalCount; ++index) {
		DrawWireBox(stageData.goals[index].position, stageData.goals[index].size, kGoalColor);
	}
	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
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
		DrawWireBox(runtimePosition, obstacle.size, color);
		if (obstacle.obstacleKind == magnet::MagnetObstacleKind::PinballBumper ||
			obstacle.obstacleKind == magnet::MagnetObstacleKind::MagneticAnchor) {
			LineDrawer::GetInstance()->DrawWireSphere(
				runtimePosition,
				(std::max)(obstacle.size.x, obstacle.size.z) * 0.5f,
				color,
				24);
		}
		if (obstacle.obstacleKind == magnet::MagnetObstacleKind::MagneticAnchor) {
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
		if (obstacle.obstacleKind == magnet::MagnetObstacleKind::RepulsionField) {
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
		DrawWireBox(object->position, object->size + padding, kSelectionColor);
	}
}

void MagnetPrototypeScene::DrawWireBox(
	const Vector3& center,
	const Vector3& size,
	const Vector4& color) const
{
	if (!IsFiniteVector3(center) || !IsFiniteVector3(size) ||
		size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
		return;
	}
	const Vector3 half = size * 0.5f;
	const Vector3 corners[8] = {
		{ center.x - half.x, center.y - half.y, center.z - half.z },
		{ center.x + half.x, center.y - half.y, center.z - half.z },
		{ center.x + half.x, center.y + half.y, center.z - half.z },
		{ center.x - half.x, center.y + half.y, center.z - half.z },
		{ center.x - half.x, center.y - half.y, center.z + half.z },
		{ center.x + half.x, center.y - half.y, center.z + half.z },
		{ center.x + half.x, center.y + half.y, center.z + half.z },
		{ center.x - half.x, center.y + half.y, center.z + half.z },
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
