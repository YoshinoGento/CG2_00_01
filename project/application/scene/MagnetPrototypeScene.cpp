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
constexpr Vector4 kConstraintColor = { 0.82f, 0.86f, 0.92f, 1.0f };
constexpr Vector4 kGridColor = { 0.18f, 0.22f, 0.28f, 1.0f };
constexpr Vector4 kVelocityColor = { 1.0f, 0.55f, 0.08f, 1.0f };
constexpr Vector4 kTestBallColor = { 1.0f, 0.8f, 0.12f, 1.0f };
constexpr Vector4 kGoalColor = { 0.25f, 1.0f, 0.35f, 1.0f };
constexpr int kGridHalfCount = 10;
constexpr float kGridSpacing = 1.0f;
constexpr float kVelocityDisplayScale = 0.22f;

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

	prototypeReady_ = magnetChainSystem_.Initialize();
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
	pendingCommand_.emitOne = pendingCommand_.emitOne || input->TriggerKey(InputKey::E);
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
		resetRequested_ = false;
		if (!prototypeReady_) {
			Logger::Log("MagnetPrototypeScene: MagnetChainSystem reset failed.");
			assert(false && "MagnetChainSystem reset failed.");
			return;
		}
	}

	magnetChainSystem_.SetPlayerCommand(pendingCommand_);
	prototypeReady_ = magnetChainSystem_.FixedUpdate(fixedDeltaTime);
	if (!prototypeReady_) {
		Logger::Log("MagnetPrototypeScene: fixed update failed; simulation disabled.");
		assert(false && "MagnetChainSystem fixed update failed.");
	}
	pendingCommand_.emergencyStop = false;
	pendingCommand_.emitOne = false;
	pendingCommand_.releaseChains = false;
}

void MagnetPrototypeScene::Update()
{
	if (camera_) {
		if (prototypeReady_ && cameraFollow_) {
			const physics::SphereBody* player = magnetChainSystem_.GetPhysicsWorld().GetBody(
				magnetChainSystem_.GetPlayerBody());
			if (player) {
				camera_->SetTranslate({ player->position.x, 11.0f, player->position.z - 16.0f });
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
	viewData.activeTestBallCount = magnetChainSystem_.GetActiveTestBallCount();
	viewData.maximumConstraintError = magnetChainSystem_.GetMaximumConstraintError();
	viewData.chainsAttached = magnetChainSystem_.AreChainsAttached();
	viewData.goalHitCount = magnetChainSystem_.GetGoalHitCount();
	viewData.goalWidth = magnetChainSystem_.GetGoal().width;
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
	pendingCommand_.emitOne = pendingCommand_.emitOne || request.emitOne;
	pendingCommand_.releaseChains = pendingCommand_.releaseChains || request.releaseChains;
	magnetChainSystem_.SetEmitterSettings(request.emitterSettings);
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
	if (showGrid_) {
		for (int index = -kGridHalfCount; index <= kGridHalfCount; ++index) {
			const float offset = static_cast<float>(index) * kGridSpacing;
			lineDrawer->DrawLine(
				{ -static_cast<float>(kGridHalfCount), 0.0f, offset },
				{ static_cast<float>(kGridHalfCount), 0.0f, offset },
				kGridColor);
			lineDrawer->DrawLine(
				{ offset, 0.0f, -static_cast<float>(kGridHalfCount) },
				{ offset, 0.0f, static_cast<float>(kGridHalfCount) },
				kGridColor);
		}
	}

	const physics::PhysicsWorld& physicsWorld = magnetChainSystem_.GetPhysicsWorld();
	DrawGoal();
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
	for (physics::BodyHandle handle : magnetChainSystem_.GetLeftChain()) {
		DrawBody(handle, kLeftChainColor);
		DrawVelocity(handle);
	}
	for (physics::BodyHandle handle : magnetChainSystem_.GetRightChain()) {
		DrawBody(handle, kRightChainColor);
		DrawVelocity(handle);
	}
	for (physics::BodyHandle handle : magnetChainSystem_.GetTestBalls()) {
		DrawBody(handle, kTestBallColor);
		DrawVelocity(handle);
	}

	lineDrawer->Draw(camera_->GetViewProjectionMatrix());
}

void MagnetPrototypeScene::DrawGoal() const
{
	const magnet::MagnetChainSystem::Goal& goal = magnetChainSystem_.GetGoal();
	const float halfWidth = goal.width * 0.5f;
	const float halfDepth = goal.depth * 0.5f;
	const float y = 0.03f;
	const Vector3 nearLeft = { goal.center.x - halfWidth, y, goal.center.z - halfDepth };
	const Vector3 nearRight = { goal.center.x + halfWidth, y, goal.center.z - halfDepth };
	const Vector3 farLeft = { goal.center.x - halfWidth, y, goal.center.z + halfDepth };
	const Vector3 farRight = { goal.center.x + halfWidth, y, goal.center.z + halfDepth };
	LineDrawer* drawer = LineDrawer::GetInstance();
	drawer->DrawLine(nearLeft, nearRight, kGoalColor);
	drawer->DrawLine(nearRight, farRight, kGoalColor);
	drawer->DrawLine(farRight, farLeft, kGoalColor);
	drawer->DrawLine(farLeft, nearLeft, kGoalColor);
	constexpr float postHeight = 1.5f;
	drawer->DrawLine(nearLeft, nearLeft + Vector3{ 0.0f, postHeight, 0.0f }, kGoalColor);
	drawer->DrawLine(nearRight, nearRight + Vector3{ 0.0f, postHeight, 0.0f }, kGoalColor);
	drawer->DrawLine(
		nearLeft + Vector3{ 0.0f, postHeight, 0.0f },
		nearRight + Vector3{ 0.0f, postHeight, 0.0f },
		kGoalColor);
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
