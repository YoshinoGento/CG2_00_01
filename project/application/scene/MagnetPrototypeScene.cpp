#include "application/scene/MagnetPrototypeScene.h"

#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "base/Framework.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "io/Input.h"

#include <algorithm>
#include <cassert>
#include <cmath>

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

[[nodiscard]] bool IsFiniteVector3(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool HasMovementInput(const Vector3& direction) noexcept
{
	return std::fabs(direction.x) > 0.0001f || std::fabs(direction.z) > 0.0001f;
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

} // namespace

void MagnetPrototypeScene::Initialize()
{
	framework_ = Framework::GetInstance();
	assert(framework_ && "Framework must exist before MagnetPrototypeScene initialization.");

	camera_ = std::make_unique<Camera>();
	camera_->SetTranslate({ 0.0f, 11.0f, -16.0f });
	camera_->SetRotate({ 0.60f, 0.0f, 0.0f });
	camera_->Update();
	LineDrawer::GetInstance()->Initialize(framework_->GetDxCommon());

	prototypeReady_ = magnetStageSystem_.Initialize() &&
		magnetChainSystem_.Initialize(magnetStageSystem_.GetStageData());
	magneticImpactFeedbackSystem_.Reset();
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
}

void MagnetPrototypeScene::Finalize()
{
	camera_.reset();
	framework_ = nullptr;
	prototypeReady_ = false;
}

void MagnetPrototypeScene::PrepareFixedUpdate()
{
	pendingCommand_.moveDirection = {};
	if (!prototypeReady_ || editorMode_ != magnet::MagnetEditorMode::Play) {
		return;
	}
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (!input || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}

	if (input->PushKey(InputKey::W)) { pendingCommand_.moveDirection.z += 1.0f; }
	if (input->PushKey(InputKey::S)) { pendingCommand_.moveDirection.z -= 1.0f; }
	if (input->PushKey(InputKey::D)) { pendingCommand_.moveDirection.x += 1.0f; }
	if (input->PushKey(InputKey::A)) { pendingCommand_.moveDirection.x -= 1.0f; }
	if (HasMovementInput(pendingCommand_.moveDirection)) {
		releaseOverviewActive_ = false;
	}
	pendingCommand_.emergencyStop =
		pendingCommand_.emergencyStop || input->TriggerKey(InputKey::Space);
	pendingCommand_.releaseChains =
		pendingCommand_.releaseChains || input->TriggerKey(InputKey::Q);
	resetRequested_ = resetRequested_ || input->TriggerKey(InputKey::R);
}

void MagnetPrototypeScene::FixedUpdate(float fixedDeltaTime)
{
	if (!prototypeReady_ || editorMode_ != magnet::MagnetEditorMode::Play) {
		pendingCommand_ = {};
		return;
	}
	if (resetRequested_) {
		prototypeReady_ = magnetChainSystem_.Reset();
		magneticImpactFeedbackSystem_.Reset();
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
		magneticImpactFeedbackSystem_.AddImpacts(
			magnetChainSystem_.GetMagneticImpactEvents(),
			magnetChainSystem_.GetMagneticImpactEventCount());
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
	}
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
		const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
		const auto& stageBalls = magnetChainSystem_.GetStageBalls();
		for (std::size_t index = 0;
			index < magnetChainSystem_.GetStageBallCount() &&
			viewData.offscreenMagnetCount < viewData.offscreenMagnetOffsets.size();
			++index) {
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
			const Vector3 offset = ball->position - player->position;
			viewData.offscreenMagnetOffsets[viewData.offscreenMagnetCount++] =
				{ offset.x, offset.z };
		}
	}

	const magnet::MagnetPrototypeUiRequest request = prototypeWindow_.Draw(
		viewData,
		context.srvManager,
		context.finalDisplaySrvIndex,
		context.virtualWidth,
		context.virtualHeight);
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
		Vector4 color = kAvailableBallColor;
		switch (stageBallStates[index]) {
		case magnet::MagnetChainSystem::StageBallState::AttachedLeft:
			color = kLeftChainColor;
			break;
		case magnet::MagnetChainSystem::StageBallState::AttachedRight:
			color = kRightChainColor;
			break;
		case magnet::MagnetChainSystem::StageBallState::Released:
			color = kReleasedBallColor;
			break;
		case magnet::MagnetChainSystem::StageBallState::Inactive:
			continue;
		case magnet::MagnetChainSystem::StageBallState::Available:
		default:
			break;
		}
		DrawBody(stageBalls[index], color);
		DrawVelocity(stageBalls[index]);
	}
	magneticImpactFeedbackSystem_.Draw(*lineDrawer);
	DrawStageObjects();
	DrawSelectionHighlight();

	lineDrawer->Draw(camera_->GetViewProjectionMatrix());
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

void MagnetPrototypeScene::DrawBody(physics::BodyHandle handle, const Vector4& color) const
{
	const physics::SphereBody* body = magnetChainSystem_.GetPhysicsWorld().GetBody(handle);
	if (body && body->active) {
		LineDrawer::GetInstance()->DrawWireSphere(body->position, body->radius, color, 18);
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
