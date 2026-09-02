#include "application/scene/MagnetPrototypeScene.h"

#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "base/Framework.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "io/Input.h"

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
constexpr int kGridHalfCount = 10;
constexpr float kGridSpacing = 1.0f;
constexpr float kVelocityDisplayScale = 0.22f;
constexpr int kArenaWallSegments = 64;
constexpr float kArenaWallHeight = 1.4f;

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
	if (!prototypeReady_) {
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
	pendingCommand_.emergencyStop =
		pendingCommand_.emergencyStop || input->TriggerKey(InputKey::Space);
	pendingCommand_.releaseChains =
		pendingCommand_.releaseChains || input->TriggerKey(InputKey::Q);
	resetRequested_ = resetRequested_ || input->TriggerKey(InputKey::R);
}

void MagnetPrototypeScene::FixedUpdate(float fixedDeltaTime)
{
	if (!prototypeReady_) {
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
	pendingCommand_.emergencyStop = false;
	pendingCommand_.releaseChains = false;
}

void MagnetPrototypeScene::Update()
{
	if (camera_) {
		if (prototypeReady_ && cameraFollow_) {
			const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
				magnetChainSystem_.GetPlayerBody());
			if (player) {
				camera_->SetTranslate(
					Vector3{ player->position.x, 11.0f, player->position.z - 16.0f } +
					magneticImpactFeedbackSystem_.GetCameraShakeOffset());
			}
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
	viewData.stageData = &magnetStageSystem_.GetStageData();
	viewData.saveEntries = magnetStageSystem_.GetSaveEntries().data();
	viewData.saveEntryCount = magnetStageSystem_.GetSaveEntryCount();
	viewData.stageOperationMessage = magnetStageSystem_.GetLastOperationMessage().c_str();
	viewData.stageOperationSucceeded = magnetStageSystem_.DidLastOperationSucceed();
	viewData.stageDirty = magnetStageSystem_.IsDirty();
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
	resetRequested_ = resetRequested_ || request.reset;
	pendingCommand_.emergencyStop = pendingCommand_.emergencyStop || request.emergencyStop;
	pendingCommand_.releaseChains = pendingCommand_.releaseChains || request.releaseChains;
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
		for (int index = -kGridHalfCount; index <= kGridHalfCount; ++index) {
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
		if (bodyA && bodyB) {
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

	lineDrawer->Draw(camera_->GetViewProjectionMatrix());
}

void MagnetPrototypeScene::ProcessStageEditorRequest(
	const magnet::MagnetPrototypeUiRequest& request)
{
	bool stageChanged = false;
	switch (request.stageAction) {
	case magnet::MagnetStageEditorAction::GenerateBalanced:
		stageChanged = magnetStageSystem_.GenerateBalanced(request.generationSettings);
		break;
	case magnet::MagnetStageEditorAction::AddBall:
		stageChanged = magnetStageSystem_.AddBall(request.editedBallPosition);
		break;
	case magnet::MagnetStageEditorAction::RemoveBall:
		stageChanged = magnetStageSystem_.RemoveBall(request.selectedBallId);
		break;
	case magnet::MagnetStageEditorAction::MoveBall:
		stageChanged = magnetStageSystem_.SetBallPosition(
			request.selectedBallId,
			request.editedBallPosition);
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
