#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/BallMomentumTracker.h"
#include "application/magnet/system/MagnetChainSystem.h"
#include "application/magnet/system/MagneticImpactAttachmentSystem.h"
#include "application/magnet/system/SpinChargeController.h"
#include "io/JsonFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {

constexpr float kFixedDeltaTime = 1.0f / 60.0f;
constexpr int kCollectionSettleSteps = 120;
constexpr int kDriveStepCount = 120;
constexpr int kStopObservationStepCount = 180;
constexpr int kReleaseTravelStepCount = 30;
constexpr float kMaximumAllowedConstraintError = 0.08f;
constexpr float kMaximumReleaseCorrectionRadians = 0.21f;
constexpr float kMinimumRootReleaseTravel = 0.50f;

bool IsFinite(const Vector3& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const Quaternion& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
		std::isfinite(value.z) && std::isfinite(value.w);
}

float QuaternionLengthSquared(const Quaternion& value)
{
	return value.x * value.x + value.y * value.y +
		value.z * value.z + value.w * value.w;
}

float DistanceXZ(const Vector3& a, const Vector3& b)
{
	const float deltaX = b.x - a.x;
	const float deltaZ = b.z - a.z;
	return std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
}

bool ApproximatelyEqual(const Vector3& a, const Vector3& b, float epsilon = 1.0e-5f)
{
	return std::abs(a.x - b.x) <= epsilon &&
		std::abs(a.y - b.y) <= epsilon &&
		std::abs(a.z - b.z) <= epsilon;
}

magnet::MagnetStageData BuildPickupStage()
{
	magnet::MagnetStageData stage{};
	stage.name = "pickup_test";
	stage.generation.minimumX = -10.0f;
	stage.generation.maximumX = 10.0f;
	stage.generation.minimumZ = -10.0f;
	stage.generation.maximumZ = 10.0f;
	stage.ballCount = magnet::MagnetChainSystem::kLinksPerSide * 2;
	stage.generation.ballCount = stage.ballCount;
	const std::array<Vector3, 8> positions = {
		Vector3{ -1.05f, 0.5f, 0.10f },
		Vector3{  1.05f, 0.5f, -0.10f },
		Vector3{ -1.35f, 0.5f, -0.12f },
		Vector3{  1.35f, 0.5f, 0.12f },
		Vector3{ -1.65f, 0.5f, 0.14f },
		Vector3{  1.65f, 0.5f, -0.14f },
		Vector3{ -1.95f, 0.5f, -0.16f },
		Vector3{  1.95f, 0.5f, 0.16f },
	};
	for (std::size_t index = 0; index < stage.ballCount; ++index) {
		stage.balls[index] = {
			static_cast<uint32_t>(index + 1),
			positions[index],
		};
	}
	return stage;
}

magnet::MagnetStageData BuildCapacityStage()
{
	magnet::MagnetStageData stage = BuildPickupStage();
	stage.name = "capacity_test";
	stage.ballCount = 10;
	stage.generation.ballCount = stage.ballCount;
	stage.balls[8] = { 9u, { -0.80f, 0.5f, 1.60f } };
	stage.balls[9] = { 10u, { 0.80f, 0.5f, 1.60f } };
	return stage;
}

bool ValidateFiniteBodies(const magnet::MagnetChainSystem& system)
{
	const physics::PhysicsWorld& world = system.GetPhysicsWorld();
	const physics::SphereBody* player = world.GetBody(system.GetPlayerBody());
	if (!player || !IsFinite(player->position) || !IsFinite(player->linearVelocity) ||
		!IsFinite(player->angularVelocity) || !IsFinite(player->orientation)) {
		return false;
	}
	for (std::size_t index = 0; index < system.GetStageBallCount(); ++index) {
		const physics::SphereBody* body = world.GetBody(system.GetStageBalls()[index]);
		if (!body || !IsFinite(body->position) || !IsFinite(body->linearVelocity) ||
			!IsFinite(body->angularVelocity) || !IsFinite(body->orientation)) {
			return false;
		}
	}
	return true;
}

bool CollectAllBalls(magnet::MagnetChainSystem& system)
{
	for (int step = 0; step < kCollectionSettleSteps; ++step) {
		system.SetPlayerCommand({});
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			return false;
		}
	}
	return system.GetLeftChainCount() == magnet::MagnetChainSystem::kLinksPerSide &&
		system.GetRightChainCount() == magnet::MagnetChainSystem::kLinksPerSide &&
		system.GetAvailableBallCount() == 0 &&
		system.GetPhysicsWorld().GetActiveConstraintCount() == 14;
}

float GetMinimumPairDistance(const magnet::MagnetStageData& stage)
{
	float minimumDistance = INFINITY;
	for (std::size_t index = 0; index < stage.ballCount; ++index) {
		for (std::size_t other = index + 1; other < stage.ballCount; ++other) {
			minimumDistance = (std::min)(
				minimumDistance,
				DistanceXZ(stage.balls[index].position, stage.balls[other].position));
		}
	}
	return minimumDistance;
}

} // namespace

int main()
{
	physics::PhysicsWorld rollingWorld;
	physics::SphereBodyDesc rollingDesc{};
	rollingDesc.position = { 0.0f, 2.0f, 0.0f };
	rollingDesc.linearVelocity = { 3.0f, -1.0f, 0.0f };
	rollingDesc.radius = 0.5f;
	rollingDesc.linearDamping = 0.05f;
	rollingDesc.gravityScale = 1.0f;
	rollingDesc.groundHeight = 0.0f;
	rollingDesc.restitution = 0.5f;
	rollingDesc.groundFriction = 0.2f;
	rollingDesc.lockToHorizontalPlane = false;
	rollingDesc.collideWithGround = true;
	const physics::BodyHandle rollingBody = rollingWorld.CreateSphereBody(rollingDesc);
	bool observedGroundBounce = false;
	for (int step = 0; step < 180; ++step) {
		if (!rollingBody.IsValid() || !rollingWorld.Step(kFixedDeltaTime, 3, 4)) {
			std::cerr << "Rolling-body simulation failed.\n";
			return 135;
		}
		const physics::SphereBody* body = rollingWorld.GetBody(rollingBody);
		if (!body || body->position.y < body->radius - 1.0e-4f ||
			!IsFinite(body->orientation) ||
			std::abs(QuaternionLengthSquared(body->orientation) - 1.0f) > 1.0e-3f) {
			std::cerr << "Rolling body penetrated the floor or lost a finite rotation.\n";
			return 136;
		}
		observedGroundBounce = observedGroundBounce || body->linearVelocity.y > 0.5f;
	}
	const physics::SphereBody* rolledBody = rollingWorld.GetBody(rollingBody);
	if (!observedGroundBounce || !rolledBody ||
		std::abs(rolledBody->orientation.w - 1.0f) < 1.0e-3f ||
		std::abs(rolledBody->angularVelocity.z) < 0.1f) {
		std::cerr << "Sphere did not produce bounded bounce and rolling rotation.\n";
		return 137;
	}
	if (!rollingWorld.SetHorizontalPlaneLock(rollingBody, true, 0.5f)) {
		std::cerr << "Rolling body could not return to plane-locked motion.\n";
		return 138;
	}
	rolledBody = rollingWorld.GetBody(rollingBody);
	if (!rolledBody || !rolledBody->lockToHorizontalPlane ||
		std::abs(rolledBody->position.y - 0.5f) > 1.0e-5f ||
		std::abs(rolledBody->linearVelocity.y) > 1.0e-5f) {
		std::cerr << "Plane-lock transition did not restore grounded motion.\n";
		return 139;
	}
	std::cout << "rolling_ground_checks=passed\n";

	magnet::MagnetChainSystem releaseTransitionSystem;
	if (!releaseTransitionSystem.Initialize(BuildPickupStage()) ||
		!CollectAllBalls(releaseTransitionSystem)) {
		std::cerr << "Released-ground-motion setup failed.\n";
		return 142;
	}
	magnet::MagnetChainSystem::PlayerCommand releaseDriveCommand{};
	releaseDriveCommand.moveDirection = { 1.0f, 0.0f, 0.0f };
	for (int step = 0; step < 45; ++step) {
		releaseTransitionSystem.SetPlayerCommand(releaseDriveCommand);
		if (!releaseTransitionSystem.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Released-ground-motion drive failed.\n";
			return 143;
		}
	}
	releaseDriveCommand = {};
	releaseDriveCommand.releaseChains = true;
	releaseTransitionSystem.SetPlayerCommand(releaseDriveCommand);
	if (!releaseTransitionSystem.FixedUpdate(kFixedDeltaTime) ||
		releaseTransitionSystem.GetReleasedBallCount() == 0) {
		std::cerr << "Released-ground-motion transition failed.\n";
		return 144;
	}
	bool releaseHasLift = false;
	for (std::size_t index = 0;
		index < releaseTransitionSystem.GetStageBallCount();
		++index) {
		if (releaseTransitionSystem.GetStageBallStates()[index] !=
			magnet::MagnetChainSystem::StageBallState::Released) {
			continue;
		}
		const physics::SphereBody* body =
			releaseTransitionSystem.GetPhysicsWorld().GetBody(
				releaseTransitionSystem.GetStageBalls()[index]);
		if (!body || body->lockToHorizontalPlane ||
			body->position.y < body->radius - 1.0e-4f) {
			std::cerr << "Released magnet remained plane locked or crossed the floor.\n";
			return 145;
		}
		releaseHasLift = releaseHasLift || body->linearVelocity.y > 0.1f;
	}
	if (!releaseHasLift) {
		std::cerr << "Moving released magnets did not receive bounded lift.\n";
		return 146;
	}
	std::array<bool, magnet::MagnetChainSystem::kStageBallCapacity> observedFalling{};
	std::array<bool, magnet::MagnetChainSystem::kStageBallCapacity> observedRebound{};
	for (int step = 0; step < 120; ++step) {
		releaseTransitionSystem.SetPlayerCommand(releaseDriveCommand);
		if (!releaseTransitionSystem.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Released-ball bounce simulation failed.\n";
			return 147;
		}
		for (std::size_t index = 0;
			index < releaseTransitionSystem.GetStageBallCount();
			++index) {
			if (releaseTransitionSystem.GetStageBallStates()[index] !=
				magnet::MagnetChainSystem::StageBallState::Released) {
				continue;
			}
			const physics::SphereBody* body =
				releaseTransitionSystem.GetPhysicsWorld().GetBody(
					releaseTransitionSystem.GetStageBalls()[index]);
			if (!body || body->position.y < body->radius - 1.0e-4f ||
				!IsFinite(body->orientation) ||
				std::abs(QuaternionLengthSquared(body->orientation) - 1.0f) > 1.0e-3f) {
				std::cerr << "Released ball penetrated the floor or lost its rotation.\n";
				return 148;
			}
			if (body->linearVelocity.y < -0.2f) {
				observedFalling[index] = true;
			} else if (observedFalling[index] && body->linearVelocity.y > 0.2f) {
				observedRebound[index] = true;
			}
		}
	}
	if (std::none_of(observedRebound.begin(), observedRebound.end(),
		[](bool value) { return value; })) {
		std::cerr << "Released balls did not rebound after falling.\n";
		return 149;
	}
	std::cout << "released_ground_transition_checks=passed\n";

	physics::PhysicsWorld attachmentWorld;
	magnet::MagneticImpactAttachmentSystem attachmentSystem;
	magnet::MagneticImpactAttachmentSystem::Settings attachmentSettings{};
	attachmentSettings.releaseGraceSeconds = 0.0f;
	attachmentSettings.minimumImpactSpeed = 1.0f;
	attachmentSystem.SetSettings(attachmentSettings);
	magnet::MagneticImpactAttachmentSystem::MagnetHandles attachmentMagnets{};
	for (std::size_t index = 0; index < attachmentMagnets.size(); ++index) {
		physics::SphereBodyDesc desc{};
		desc.position = { static_cast<float>(index) * 10.0f, 0.5f, 0.0f };
		if (index == 0) {
			desc.position.x = 0.0f;
			desc.linearVelocity.x = 2.0f;
		} else if (index == 1) {
			desc.position.x = 0.95f;
			desc.linearVelocity.x = -2.0f;
		}
		attachmentMagnets[index] = attachmentWorld.CreateSphereBody(desc);
		if (!attachmentMagnets[index].IsValid()) {
			std::cerr << "Attachment test body creation failed.\n";
			return 109;
		}
	}
	attachmentSystem.BeginRelease();
	if (!attachmentSystem.Update(attachmentWorld, attachmentMagnets, kFixedDeltaTime) ||
		attachmentSystem.GetAttachmentCount() != 1 ||
		attachmentWorld.GetActiveConstraintCount() != 1) {
		std::cerr << "Magnetic impact did not create exactly one attachment.\n";
		return 110;
	}

	magnet::SpinChargeController spinChargeController;
	magnet::SpinChargeController::Settings spinSettings{};
	spinSettings.enabled = true;
	spinSettings.rotationsForFullCharge = 1.0f;
	spinSettings.maximumTurnSpeedMultiplier = 4.0f;
	spinSettings.maximumSpeedMultiplier = 8.0f;
	spinSettings.maximumLaunchSpeed = 96.0f;
	spinSettings.minimumBallSpeedForBoost = 1.5f;
	spinSettings.ballSpeedForFullBoost = 10.0f;
	spinChargeController.SetSettings(spinSettings);
	if (!spinChargeController.Update(0.0f, kFixedDeltaTime)) {
		std::cerr << "Spin charge initialization failed.\n";
		return 111;
	}
	for (int step = 1; step <= 120; ++step) {
		const float heading = static_cast<float>(step) * 0.0523598776f;
		if (!spinChargeController.Update(heading, kFixedDeltaTime)) {
			std::cerr << "Spin charge update failed.\n";
			return 112;
		}
	}
	const Vector3 chargedVelocity = spinChargeController.ApplyToLaunchVelocity(
		{ 0.0f, 0.0f, 10.0f });
	const Vector3 nearlyStationaryChargedVelocity =
		spinChargeController.ApplyToLaunchVelocity({ 0.0f, 0.0f, 0.5f });
	if (spinChargeController.GetChargeRatio() < 0.99f ||
		spinChargeController.GetTurnSpeedMultiplier() < 3.99f ||
		spinChargeController.GetSpeedMultiplier() < 7.99f ||
		DistanceXZ({}, chargedVelocity) < 79.9f ||
		DistanceXZ({}, nearlyStationaryChargedVelocity) > 0.501f) {
		std::cerr << "Full spin charge did not produce the expected launch boost.\n";
		return 113;
	}
	spinChargeController.ResetCharge();
	if (spinChargeController.GetChargeRatio() != 0.0f ||
		spinChargeController.GetTurnSpeedMultiplier() != 1.0f ||
		spinChargeController.GetSpeedMultiplier() != 1.0f) {
		std::cerr << "Spin charge was not consumed after release.\n";
		return 114;
	}

	magnet::BallMomentumTracker momentumTracker;
	for (int step = 0; step < 30; ++step) {
		if (!momentumTracker.Update(0, { 0.0f, 0.0f, 4.0f }, kFixedDeltaTime)) {
			std::cerr << "Low-speed momentum tracking failed.\n";
			return 1;
		}
	}
	const float lowMomentumSpeed = DistanceXZ(
		{},
		momentumTracker.CalculateLaunchVelocity(0, { 0.0f, 0.0f, 4.0f }));
	momentumTracker.Reset();
	for (int step = 0; step < 30; ++step) {
		if (!momentumTracker.Update(0, { 0.0f, 0.0f, 14.0f }, kFixedDeltaTime)) {
			std::cerr << "High-speed momentum tracking failed.\n";
			return 2;
		}
	}
	const float highMomentumSpeed = DistanceXZ(
		{},
		momentumTracker.CalculateLaunchVelocity(0, { 0.0f, 0.0f, 14.0f }));
	if (!std::isfinite(lowMomentumSpeed) || !std::isfinite(highMomentumSpeed) ||
		highMomentumSpeed <= lowMomentumSpeed * 3.5f || highMomentumSpeed > 22.001f) {
		std::cerr << "Momentum launch scaling is invalid.\n";
		return 3;
	}

	magnet::MagnetStageGenerationSettings generation{};
	generation.seed = 123456u;
	generation.ballCount = 16;
	generation.minimumSpacing = 2.0f;
	generation.playerClearRadius = 3.0f;
	magnet::MagnetStageSystem stageSystem;
	magnet::MagnetStageSystem repeatedStageSystem;
	if (!stageSystem.GenerateBalanced(generation) ||
		!repeatedStageSystem.GenerateBalanced(generation)) {
		std::cerr << "Balanced random generation failed.\n";
		return 4;
	}
	const magnet::MagnetStageData& generated = stageSystem.GetStageData();
	const magnet::MagnetStageData& repeated = repeatedStageSystem.GetStageData();
	std::array<int, 4> quadrantCounts{};
	for (std::size_t index = 0; index < generated.ballCount; ++index) {
		const Vector3 position = generated.balls[index].position;
		const int quadrant = (position.x >= 0.0f ? 1 : 0) +
			(position.z >= 0.0f ? 2 : 0);
		++quadrantCounts[quadrant];
		if (DistanceXZ({}, position) < generation.playerClearRadius ||
			DistanceXZ(position, repeated.balls[index].position) > 1.0e-5f) {
			std::cerr << "Generated layout is unsafe or not seed-reproducible.\n";
			return 5;
		}
	}
	if (!std::all_of(
		quadrantCounts.begin(),
		quadrantCounts.end(),
		[](int count) { return count == 4; }) ||
		GetMinimumPairDistance(generated) < generation.minimumSpacing) {
		std::cerr << "Generated layout is visibly biased or too tightly packed.\n";
		return 6;
	}
	std::cout << "generated_minimum_pair_distance="
		<< GetMinimumPairDistance(generated) << '\n';
	const Vector3 authoredPlayerPosition{ 2.5f, 0.75f, -1.5f };
	if (!stageSystem.SetPlayerPosition(authoredPlayerPosition) ||
		stageSystem.SetPlayerPosition({ 20.0f, 0.75f, 0.0f })) {
		std::cerr << "Player start-position validation failed.\n";
		return 115;
	}
	const Vector3 goalPosition{ 0.0f, 1.0f, 8.0f };
	const Vector3 goalSize{ 4.0f, 2.0f, 1.0f };
	const Vector3 obstaclePosition{ -3.0f, 1.0f, 2.0f };
	const Vector3 obstacleSize{ 1.5f, 2.0f, 3.0f };
	const Vector3 editedGoalPosition{ 1.0f, 1.5f, 7.5f };
	const Vector3 editedGoalSize{ 3.5f, 2.5f, 1.5f };
	const Vector3 editedObstaclePosition{ -2.5f, 1.25f, 2.5f };
	const Vector3 editedObstacleSize{ 2.0f, 2.5f, 2.0f };
	if (!stageSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Goal,
			goalPosition,
			goalSize) ||
		!stageSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			obstaclePosition,
			obstacleSize,
			magnet::MagnetObstacleKind::Chainsaw) ||
		stageSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			obstaclePosition,
			{ 0.0f, 2.0f, 3.0f }) ||
		!stageSystem.SetBoxObjectTransform(
			magnet::MagnetStageObjectType::Goal,
			1u,
			editedGoalPosition,
			editedGoalSize) ||
		!stageSystem.SetBoxObjectTransform(
			magnet::MagnetStageObjectType::Obstacle,
			1u,
			editedObstaclePosition,
			editedObstacleSize) ||
		!stageSystem.SetObstacleKind(
			1u,
			magnet::MagnetObstacleKind::TransferGate) ||
		!stageSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			{ 3.0f, 1.25f, 2.5f },
			{ 0.55f, 2.5f, 3.0f },
			magnet::MagnetObstacleKind::TransferGate) ||
		!stageSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			{ 0.0f, 1.0f, 6.0f },
			{ 6.0f, 2.0f, 6.0f },
			magnet::MagnetObstacleKind::RepulsionField) ||
		stageSystem.SetTransferPairId(2u, 0u) ||
		!stageSystem.GenerateBalanced(generation) ||
		stageSystem.GetStageData().goalCount != 1 ||
		stageSystem.GetStageData().obstacleCount != 3 ||
		!ApproximatelyEqual(
			stageSystem.GetStageData().goals[0].position,
			editedGoalPosition) ||
		!ApproximatelyEqual(
			stageSystem.GetStageData().obstacles[0].position,
			editedObstaclePosition) ||
		stageSystem.GetStageData().obstacles[0].obstacleKind !=
			magnet::MagnetObstacleKind::TransferGate ||
		stageSystem.GetStageData().obstacles[0].transferPairId == 0 ||
		stageSystem.GetStageData().obstacles[0].transferPairId !=
			stageSystem.GetStageData().obstacles[1].transferPairId ||
		stageSystem.GetStageData().obstacles[2].obstacleKind !=
			magnet::MagnetObstacleKind::RepulsionField ||
		stageSystem.GetStageData().obstacles[2].transferPairId != 0 ||
		DistanceXZ(
			stageSystem.GetStageData().playerPosition,
			authoredPlayerPosition) > 1.0e-5f) {
		std::cerr << "Goal/Obstacle validation or generator preservation failed.\n";
		return 107;
	}
	magnet::MagnetStageSystem defaultStageSystem;
	if (!defaultStageSystem.Load("project/Resources/levels/magnet/stage_01.json") ||
		defaultStageSystem.GetStageData().ballCount != 16 ||
		defaultStageSystem.GetStageData().goalCount != 1 ||
		defaultStageSystem.GetStageData().obstacleCount != 2 ||
		defaultStageSystem.GetStageData().obstacles[0].obstacleKind !=
			magnet::MagnetObstacleKind::Solid ||
		defaultStageSystem.GetStageData().obstacles[1].obstacleKind !=
			magnet::MagnetObstacleKind::Solid ||
		DistanceXZ(defaultStageSystem.GetStageData().playerPosition, {}) > 1.0e-5f) {
		std::cerr << "Tracked default stage JSON is invalid.\n";
		return 7;
	}
	magnet::MagnetChainSystem authoredGoalSystem;
	const magnet::MagnetStageBoxPlacement& authoredGoal =
		defaultStageSystem.GetStageData().goals[0];
	if (!authoredGoalSystem.Initialize(defaultStageSystem.GetStageData()) ||
		DistanceXZ(authoredGoalSystem.GetGoal().center, authoredGoal.position) > 1.0e-5f ||
		std::abs(authoredGoalSystem.GetGoal().width - authoredGoal.size.x) > 1.0e-5f ||
		std::abs(authoredGoalSystem.GetGoal().depth - authoredGoal.size.z) > 1.0e-5f) {
		std::cerr << "Runtime Goal does not match the authored stage Goal.\n";
		return 108;
	}

	const std::string roundTripPath = "generated/tests/magnet_stage_roundtrip.json";
	if (!stageSystem.Save(roundTripPath)) {
		std::cerr << "Stage JSON save failed.\n";
		return 8;
	}
	magnet::MagnetStageSystem loadedStageSystem;
	if (!loadedStageSystem.Load(roundTripPath) ||
		loadedStageSystem.GetStageData().ballCount != generated.ballCount ||
		loadedStageSystem.GetStageData().goalCount != 1 ||
		loadedStageSystem.GetStageData().obstacleCount != 3 ||
		DistanceXZ(
			loadedStageSystem.GetStageData().playerPosition,
			authoredPlayerPosition) > 1.0e-5f ||
		!ApproximatelyEqual(
			loadedStageSystem.GetStageData().goals[0].position,
			editedGoalPosition) ||
		!ApproximatelyEqual(
			loadedStageSystem.GetStageData().goals[0].size,
			editedGoalSize) ||
		!ApproximatelyEqual(
			loadedStageSystem.GetStageData().obstacles[0].position,
			editedObstaclePosition) ||
		!ApproximatelyEqual(
			loadedStageSystem.GetStageData().obstacles[0].size,
			editedObstacleSize) ||
		loadedStageSystem.GetStageData().obstacles[0].obstacleKind !=
			magnet::MagnetObstacleKind::TransferGate ||
		loadedStageSystem.GetStageData().obstacles[0].transferPairId == 0 ||
		loadedStageSystem.GetStageData().obstacles[0].transferPairId !=
			loadedStageSystem.GetStageData().obstacles[1].transferPairId ||
		loadedStageSystem.GetStageData().obstacles[2].obstacleKind !=
			magnet::MagnetObstacleKind::RepulsionField ||
		loadedStageSystem.GetStageData().obstacles[2].transferPairId != 0) {
		std::cerr << "Stage JSON load failed.\n";
		return 9;
	}
	magnet::MagnetChainSystem placedPlayerSystem;
	if (!placedPlayerSystem.Initialize(loadedStageSystem.GetStageData())) {
		std::cerr << "Runtime rejected the authored Player position.\n";
		return 116;
	}
	const physics::SphereBody* placedPlayer = placedPlayerSystem.GetPhysicsWorld().GetBody(
		placedPlayerSystem.GetPlayerBody());
	if (!placedPlayer ||
		DistanceXZ(placedPlayer->position, authoredPlayerPosition) > 1.0e-5f) {
		std::cerr << "Runtime Player did not use the authored start position.\n";
		return 117;
	}
	magnet::MagnetChainSystem::PlayerCommand movePlacedPlayer{};
	movePlacedPlayer.moveDirection = { 1.0f, 0.0f, 0.0f };
	placedPlayerSystem.SetPlayerCommand(movePlacedPlayer);
	if (!placedPlayerSystem.FixedUpdate(kFixedDeltaTime) ||
		!placedPlayerSystem.Reset()) {
		std::cerr << "Runtime Player reset failed.\n";
		return 118;
	}
	placedPlayer = placedPlayerSystem.GetPhysicsWorld().GetBody(
		placedPlayerSystem.GetPlayerBody());
	if (!placedPlayer ||
		DistanceXZ(placedPlayer->position, authoredPlayerPosition) > 1.0e-5f) {
		std::cerr << "Reset did not restore the authored Player start position.\n";
		return 119;
	}
	std::error_code removeError;
	std::filesystem::remove(roundTripPath, removeError);
	if (removeError) {
		std::cerr << "Stage round-trip cleanup failed.\n";
		return 10;
	}

	const std::string legacyStagePath = "generated/tests/magnet_stage_v2.json";
	const nlohmann::json legacyStage = {
		{ "schema", "magnet_stage" },
		{ "schemaVersion", 2u },
		{ "name", "legacy_v2" },
		{ "bounds", {
			{ "minimumX", -9.0f }, { "maximumX", 9.0f },
			{ "minimumZ", -9.0f }, { "maximumZ", 9.0f } } },
		{ "generator", {
			{ "seed", 1u }, { "minimumSpacing", 2.0f },
			{ "playerClearRadius", 3.0f } } },
		{ "objects", nlohmann::json::array() },
	};
	magnet::MagnetStageSystem legacyStageSystem;
	if (!JsonFile::Save(legacyStagePath, legacyStage) ||
		!legacyStageSystem.Load(legacyStagePath) ||
		DistanceXZ(legacyStageSystem.GetStageData().playerPosition, {}) > 1.0e-5f) {
		std::cerr << "Schema-v2 Player fallback is incompatible.\n";
		return 120;
	}
	removeError.clear();
	std::filesystem::remove(legacyStagePath, removeError);
	if (removeError) {
		std::cerr << "Legacy-stage cleanup failed.\n";
		return 121;
	}

	physics::PhysicsWorld obstacleWorld;
	physics::SphereBodyDesc obstacleBallDesc{};
	obstacleBallDesc.position = { 1.2f, 0.5f, 0.0f };
	obstacleBallDesc.linearVelocity = { -5.0f, 0.0f, 0.0f };
	const physics::BodyHandle obstacleBall =
		obstacleWorld.CreateSphereBody(obstacleBallDesc);
	std::array<physics::BodyHandle, 1> obstacleBalls{ obstacleBall };
	magnet::MagnetStageBoxPlacement gimmick{};
	gimmick.id = 1u;
	gimmick.position = { 0.0f, 0.5f, 0.0f };
	gimmick.size = { 2.0f, 1.0f, 2.0f };
	gimmick.obstacleKind = magnet::MagnetObstacleKind::PinballBumper;
	magnet::ObstacleCollisionSystem obstacleSystem;
	if (!obstacleBall.IsValid() ||
		!obstacleSystem.Resolve(
			obstacleWorld,
			{},
			obstacleBalls.data(),
			obstacleBalls.size(),
			&gimmick,
			1,
			kFixedDeltaTime)) {
		std::cerr << "Pinball-bumper resolution failed.\n";
		return 140;
	}
	const physics::SphereBody* obstacleBody = obstacleWorld.GetBody(obstacleBall);
	if (!obstacleBody || obstacleBody->linearVelocity.x < 8.49f) {
		std::cerr << "Pinball bumper did not produce a boosted reflection.\n";
		return 141;
	}

	obstacleSystem.Reset();
	gimmick.obstacleKind = magnet::MagnetObstacleKind::Chainsaw;
	if (!obstacleWorld.SetPosition(obstacleBall, gimmick.position) ||
		!obstacleSystem.Resolve(
			obstacleWorld, {}, obstacleBalls.data(), obstacleBalls.size(),
			&gimmick, 1, kFixedDeltaTime) ||
		obstacleSystem.GetEventCount() != 1 ||
		obstacleSystem.GetEvents()[0].type !=
			magnet::ObstacleCollisionSystem::EventType::CutChain) {
		std::cerr << "Chainsaw did not emit one bounded cut event.\n";
		return 142;
	}
	obstacleSystem.Reset();
	gimmick.obstacleKind = magnet::MagnetObstacleKind::Furnace;
	if (!obstacleWorld.SetPosition(obstacleBall, gimmick.position) ||
		!obstacleSystem.Resolve(
			obstacleWorld, {}, obstacleBalls.data(), obstacleBalls.size(),
			&gimmick, 1, kFixedDeltaTime) ||
		obstacleSystem.GetEventCount() != 1 ||
		obstacleSystem.GetEvents()[0].type !=
			magnet::ObstacleCollisionSystem::EventType::DissolveBall) {
		std::cerr << "Furnace did not emit one bounded dissolve event.\n";
		return 143;
	}

	obstacleSystem.Reset();
	gimmick.obstacleKind = magnet::MagnetObstacleKind::TimedShutter;
	if (!obstacleSystem.Resolve(
			obstacleWorld, {}, nullptr, 0, &gimmick, 1, kFixedDeltaTime) ||
		!obstacleSystem.IsShutterClosed(0) ||
		obstacleSystem.GetShutterOpenRatio(0) != 0.0f ||
		obstacleSystem.GetShutterVerticalOffset(0, gimmick) != 0.0f) {
		std::cerr << "Timed shutter did not start closed.\n";
		return 144;
	}
	for (int step = 0; step < 120; ++step) {
		if (!obstacleSystem.Resolve(
			obstacleWorld, {}, nullptr, 0, &gimmick, 1, kFixedDeltaTime)) {
			std::cerr << "Timed shutter update failed.\n";
			return 145;
		}
	}
	if (obstacleSystem.IsShutterClosed(0) ||
		obstacleSystem.GetShutterOpenRatio(0) < 0.99f ||
		obstacleSystem.GetShutterVerticalOffset(0, gimmick) <= gimmick.size.y) {
		std::cerr << "Timed shutter did not enter its open interval.\n";
		return 146;
	}
	if (!obstacleWorld.SetPosition(obstacleBall, gimmick.position) ||
		!obstacleWorld.SetLinearVelocity(obstacleBall, {}) ||
		!obstacleSystem.Resolve(
			obstacleWorld, {}, obstacleBalls.data(), obstacleBalls.size(),
			&gimmick, 1, kFixedDeltaTime)) {
		std::cerr << "Raised shutter pass-through setup failed.\n";
		return 156;
	}
	obstacleBody = obstacleWorld.GetBody(obstacleBall);
	if (!obstacleBody || std::abs(obstacleBody->position.x) > 1.0e-5f ||
		std::abs(obstacleBody->position.z) > 1.0e-5f) {
		std::cerr << "Raised shutter still blocked a ground ball.\n";
		return 157;
	}
	for (int step = 0; step < 84; ++step) {
		if (!obstacleSystem.Resolve(
			obstacleWorld, {}, nullptr, 0, &gimmick, 1, kFixedDeltaTime)) {
			std::cerr << "Timed shutter close transition failed.\n";
			return 158;
		}
	}
	if (!obstacleSystem.IsShutterClosed(0) ||
		!obstacleWorld.SetPosition(obstacleBall, gimmick.position) ||
		!obstacleSystem.Resolve(
			obstacleWorld, {}, obstacleBalls.data(), obstacleBalls.size(),
			&gimmick, 1, kFixedDeltaTime)) {
		std::cerr << "Lowered shutter collision setup failed.\n";
		return 159;
	}
	obstacleBody = obstacleWorld.GetBody(obstacleBall);
	if (!obstacleBody || (std::abs(obstacleBody->position.x) < 1.0f &&
		std::abs(obstacleBody->position.z) < 1.0f)) {
		std::cerr << "Lowered shutter did not block a ground ball.\n";
		return 160;
	}

	obstacleSystem.Reset();
	gimmick.obstacleKind = magnet::MagnetObstacleKind::MagneticAnchor;
	if (!obstacleWorld.SetPosition(obstacleBall, { 3.0f, 0.5f, 0.0f }) ||
		!obstacleWorld.SetLinearVelocity(obstacleBall, {})) {
		std::cerr << "Magnetic-anchor test setup failed.\n";
		return 147;
	}
	for (int step = 0; step < 20; ++step) {
		if (!obstacleWorld.Step(kFixedDeltaTime, 1, 1) ||
			!obstacleSystem.Resolve(
				obstacleWorld, {}, obstacleBalls.data(), obstacleBalls.size(),
				&gimmick, 1, kFixedDeltaTime)) {
			std::cerr << "Magnetic-anchor update failed.\n";
			return 148;
		}
	}
	obstacleBody = obstacleWorld.GetBody(obstacleBall);
	if (!obstacleBody || obstacleBody->position.x >= 1.8f ||
		!obstacleSystem.GetAnchoredBody(0).IsValid()) {
		std::cerr << "Magnetic anchor did not attract the selected ball.\n";
		return 149;
	}

	physics::PhysicsWorld repulsionWorld;
	physics::SphereBodyDesc repulsionPlayerDesc{};
	repulsionPlayerDesc.position = { 0.0f, 0.75f, 0.0f };
	repulsionPlayerDesc.linearVelocity = { 3.0f, 0.0f, 0.0f };
	repulsionPlayerDesc.radius = 0.75f;
	repulsionPlayerDesc.planeHeight = 0.75f;
	repulsionPlayerDesc.motionType = physics::MotionType::Kinematic;
	const physics::BodyHandle repulsionPlayer =
		repulsionWorld.CreateSphereBody(repulsionPlayerDesc);
	physics::SphereBodyDesc repulsionBallDesc{};
	repulsionBallDesc.position = { 1.0f, 0.5f, 0.0f };
	const physics::BodyHandle attachedRepulsionBall =
		repulsionWorld.CreateSphereBody(repulsionBallDesc);
	repulsionBallDesc.position = { -4.75f, 0.5f, 0.0f };
	const physics::BodyHandle edgeRepulsionBall =
		repulsionWorld.CreateSphereBody(repulsionBallDesc);
	repulsionBallDesc.position = { 2.0f, 0.5f, 0.0f };
	repulsionBallDesc.linearVelocity = { 100.0f, 0.0f, 0.0f };
	const physics::BodyHandle fastRepulsionBall =
		repulsionWorld.CreateSphereBody(repulsionBallDesc);
	physics::DistanceConstraintDesc attachedConstraint{};
	attachedConstraint.bodyA = repulsionPlayer;
	attachedConstraint.bodyB = attachedRepulsionBall;
	attachedConstraint.restLength = 1.0f;
	std::array<physics::BodyHandle, 3> repulsionBalls{
		attachedRepulsionBall,
		edgeRepulsionBall,
		fastRepulsionBall,
	};
	magnet::MagnetStageBoxPlacement repulsionField{};
	repulsionField.id = 20u;
	repulsionField.position = { 0.0f, 1.0f, 0.0f };
	repulsionField.size = { 10.0f, 2.0f, 10.0f };
	repulsionField.obstacleKind = magnet::MagnetObstacleKind::RepulsionField;
	magnet::ObstacleCollisionSystem repulsionSystem;
	if (!repulsionPlayer.IsValid() || !attachedRepulsionBall.IsValid() ||
		!edgeRepulsionBall.IsValid() || !fastRepulsionBall.IsValid() ||
		!repulsionWorld.CreateDistanceConstraint(attachedConstraint) ||
		!repulsionSystem.Resolve(
			repulsionWorld,
			repulsionPlayer,
			repulsionBalls.data(),
			repulsionBalls.size(),
			&repulsionField,
			1,
			kFixedDeltaTime)) {
		std::cerr << "Repulsion-field update failed.\n";
		return 173;
	}
	const physics::SphereBody* repulsionPlayerBody =
		repulsionWorld.GetBody(repulsionPlayer);
	const physics::SphereBody* attachedRepulsionBody =
		repulsionWorld.GetBody(attachedRepulsionBall);
	const physics::SphereBody* edgeRepulsionBody =
		repulsionWorld.GetBody(edgeRepulsionBall);
	const physics::SphereBody* fastRepulsionBody =
		repulsionWorld.GetBody(fastRepulsionBall);
	if (!repulsionPlayerBody || !attachedRepulsionBody ||
		!edgeRepulsionBody || !fastRepulsionBody ||
		std::abs(repulsionPlayerBody->linearVelocity.x - 3.0f) > 1.0e-5f ||
		attachedRepulsionBody->linearVelocity.x < 0.9f ||
		edgeRepulsionBody->linearVelocity.x >= -0.001f ||
		std::abs(edgeRepulsionBody->linearVelocity.x) >= 0.1f ||
		std::abs(fastRepulsionBody->linearVelocity.x) > 24.01f ||
		repulsionWorld.GetActiveConstraintCount() != 1 ||
		repulsionSystem.GetEventCount() != 0) {
		std::cerr << "Repulsion field affected the Player, severed a link, or used unsafe falloff.\n";
		return 174;
	}

	physics::PhysicsWorld transferWorld;
	physics::SphereBodyDesc transferPlayerDesc{};
	transferPlayerDesc.position = { 0.0f, 0.75f, 0.0f };
	transferPlayerDesc.linearVelocity = { 4.0f, 0.0f, 1.0f };
	transferPlayerDesc.radius = 0.75f;
	transferPlayerDesc.planeHeight = 0.75f;
	transferPlayerDesc.motionType = physics::MotionType::Kinematic;
	const physics::BodyHandle transferPlayer =
		transferWorld.CreateSphereBody(transferPlayerDesc);
	physics::SphereBodyDesc transferBallDesc{};
	transferBallDesc.position = { 0.0f, 0.5f, 0.6f };
	transferBallDesc.linearVelocity = { 3.0f, 0.0f, -2.0f };
	const physics::BodyHandle transferBall =
		transferWorld.CreateSphereBody(transferBallDesc);
	std::array<physics::BodyHandle, 1> transferBalls{ transferBall };
	std::array<magnet::MagnetStageBoxPlacement, 2> transferGates{};
	transferGates[0].id = 10u;
	transferGates[0].position = { 0.0f, 1.0f, 0.0f };
	transferGates[0].size = { 0.5f, 2.0f, 4.0f };
	transferGates[0].obstacleKind = magnet::MagnetObstacleKind::TransferGate;
	transferGates[0].transferPairId = 7u;
	transferGates[1].id = 11u;
	transferGates[1].position = { 8.0f, 1.0f, 5.0f };
	transferGates[1].size = { 4.0f, 2.0f, 0.5f };
	transferGates[1].obstacleKind = magnet::MagnetObstacleKind::TransferGate;
	transferGates[1].transferPairId = 7u;
	magnet::ObstacleCollisionSystem transferSystem;
	if (!transferPlayer.IsValid() || !transferBall.IsValid() ||
		!transferSystem.Resolve(
			transferWorld,
			transferPlayer,
			transferBalls.data(),
			transferBalls.size(),
			transferGates.data(),
			transferGates.size(),
			kFixedDeltaTime) ||
		transferSystem.GetEventCount() != 2 ||
		transferSystem.GetEvents()[0].type !=
			magnet::ObstacleCollisionSystem::EventType::EnterTransferGate ||
		transferSystem.GetEvents()[0].destinationObstacleId != 11u ||
		transferSystem.GetEvents()[1].destinationObstacleId != 11u) {
		std::cerr << "Transfer gate did not emit bounded paired-entry events.\n";
		return 161;
	}
	const auto transferEvents = transferSystem.GetEvents();
	if (!transferSystem.TeleportBody(
			transferWorld,
			transferEvents[0].body,
			transferGates[0],
			transferGates[1]) ||
		!transferSystem.TeleportBody(
			transferWorld,
			transferEvents[1].body,
			transferGates[0],
			transferGates[1])) {
		std::cerr << "Transfer gate body transport failed.\n";
		return 162;
	}
	const physics::SphereBody* transferredPlayer =
		transferWorld.GetBody(transferPlayer);
	const physics::SphereBody* transferredBall =
		transferWorld.GetBody(transferBall);
	if (!transferredPlayer || !transferredBall ||
		std::abs(transferredPlayer->linearVelocity.x + 1.0f) > 1.0e-5f ||
		std::abs(transferredPlayer->linearVelocity.z - 4.0f) > 1.0e-5f ||
		std::abs(transferredBall->linearVelocity.x - 2.0f) > 1.0e-5f ||
		std::abs(transferredBall->linearVelocity.z - 3.0f) > 1.0e-5f ||
		transferredPlayer->position.z <= transferGates[1].position.z ||
		transferredBall->position.z <= transferGates[1].position.z) {
		std::cerr << "Transfer gate changed speed or used the wrong exit direction.\n";
		return 163;
	}
	if (!transferWorld.SetPosition(transferPlayer, transferGates[0].position) ||
		!transferWorld.SetPosition(transferBall, transferGates[0].position) ||
		!transferSystem.Resolve(
			transferWorld,
			transferPlayer,
			transferBalls.data(),
			transferBalls.size(),
			transferGates.data(),
			transferGates.size(),
			kFixedDeltaTime) ||
		transferSystem.GetEventCount() != 0) {
		std::cerr << "Transfer gate immediate re-entry cooldown failed.\n";
		return 164;
	}
	transferSystem.Reset();
	if (!transferSystem.Resolve(
			transferWorld,
			transferPlayer,
			transferBalls.data(),
			transferBalls.size(),
			transferGates.data(),
			1,
			kFixedDeltaTime) ||
		transferSystem.GetEventCount() != 0) {
		std::cerr << "Unpaired transfer gate should remain inactive.\n";
		return 165;
	}

	const std::filesystem::path saveBrowserDirectory =
		"generated/tests/magnet_stage_save_browser";
	const std::filesystem::path namedStagePath = saveBrowserDirectory / "alpha.json";
	std::error_code browserCleanupError;
	std::filesystem::remove(namedStagePath, browserCleanupError);
	browserCleanupError.clear();
	std::filesystem::remove(saveBrowserDirectory, browserCleanupError);
	browserCleanupError.clear();
	magnet::MagnetStageSystem saveBrowserSystem(saveBrowserDirectory.generic_string());
	if (!saveBrowserSystem.GenerateBalanced(generation) ||
		!saveBrowserSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Goal,
			goalPosition,
			goalSize) ||
		!saveBrowserSystem.AddBoxObject(
			magnet::MagnetStageObjectType::Obstacle,
			obstaclePosition,
			obstacleSize) ||
		!saveBrowserSystem.IsDirty() ||
		!saveBrowserSystem.RefreshSaveEntries() ||
		saveBrowserSystem.GetSaveEntryCount() != 0) {
		std::cerr << "Named-stage save browser initialization failed.\n";
		return 100;
	}
	const Vector3 savedFirstPosition = saveBrowserSystem.GetStageData().balls[0].position;
	if (!saveBrowserSystem.SaveNamed("alpha", false) ||
		saveBrowserSystem.IsDirty() ||
		saveBrowserSystem.GetSaveEntryCount() != 1 ||
		saveBrowserSystem.GetSaveEntries()[0].name != "alpha") {
		std::cerr << "Named-stage save/list failed.\n";
		return 101;
	}
	if (saveBrowserSystem.SaveNamed("alpha", false) ||
		saveBrowserSystem.SaveNamed("CON", false) ||
		saveBrowserSystem.SaveNamed("../escape", false)) {
		std::cerr << "Named-stage overwrite or filename validation failed.\n";
		return 102;
	}
	Vector3 editedFirstPosition = savedFirstPosition;
	editedFirstPosition.x += savedFirstPosition.x >= 0.0f ? -0.5f : 0.5f;
	if (!saveBrowserSystem.SetBallPosition(1u, editedFirstPosition) ||
		!saveBrowserSystem.IsDirty() ||
		!saveBrowserSystem.LoadNamed("alpha") ||
		saveBrowserSystem.IsDirty() ||
		saveBrowserSystem.GetStageData().goalCount != 1 ||
		saveBrowserSystem.GetStageData().obstacleCount != 1 ||
		DistanceXZ(
			saveBrowserSystem.GetStageData().balls[0].position,
			savedFirstPosition) > 1.0e-5f) {
		std::cerr << "Named-stage dirty/load transition failed.\n";
		return 103;
	}
	if (!saveBrowserSystem.SetBallPosition(1u, editedFirstPosition) ||
		!saveBrowserSystem.SaveNamed("alpha", true) ||
		saveBrowserSystem.IsDirty()) {
		std::cerr << "Confirmed named-stage overwrite failed.\n";
		return 104;
	}
	std::filesystem::remove(namedStagePath, browserCleanupError);
	if (browserCleanupError) {
		std::cerr << "Named-stage file cleanup failed.\n";
		return 105;
	}
	browserCleanupError.clear();
	std::filesystem::remove(saveBrowserDirectory, browserCleanupError);
	if (browserCleanupError) {
		std::cerr << "Named-stage directory cleanup failed.\n";
		return 106;
	}

	magnet::MagnetChainSystem capacitySystem;
	const magnet::MagnetStageData capacityStage = BuildCapacityStage();
	if (!capacitySystem.Initialize(capacityStage)) {
		std::cerr << "Capacity-stage initialization failed.\n";
		return 11;
	}
	for (int step = 0; step < kCollectionSettleSteps; ++step) {
		capacitySystem.SetPlayerCommand({});
		if (!capacitySystem.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Capacity-stage collection failed.\n";
			return 12;
		}
	}
	if (capacitySystem.GetAttachedBallCount() != 8 ||
		capacitySystem.GetAvailableBallCount() != 2 ||
		capacitySystem.GetPhysicsWorld().GetActiveConstraintCount() != 14) {
		std::cerr << "Chain capacity did not leave excess stage balls loose.\n";
		return 13;
	}
	magnet::MagnetChainSystem::PlayerCommand capacityRelease{};
	capacityRelease.releaseChains = true;
	capacitySystem.SetPlayerCommand(capacityRelease);
	if (!capacitySystem.FixedUpdate(kFixedDeltaTime) ||
		capacitySystem.GetReleasedBallCount() != 8 ||
		capacitySystem.GetAvailableBallCount() != 2 ||
		capacitySystem.GetPhysicsWorld().GetActiveConstraintCount() != 0) {
		std::cerr << "Release changed balls that were never attached.\n";
		return 14;
	}
	for (int step = 0; step < 4; ++step) {
		capacitySystem.SetPlayerCommand({});
		if (!capacitySystem.FixedUpdate(kFixedDeltaTime)) {
			return 15;
		}
	}
	if (capacitySystem.GetAttachedBallCount() != 2 ||
		capacitySystem.GetAvailableBallCount() != 0 ||
		capacitySystem.GetPhysicsWorld().GetActiveConstraintCount() == 0) {
		std::cerr << "Released constraint slots were not reusable.\n";
		return 16;
	}

	magnet::MagnetStageData chainsawStage = BuildPickupStage();
	chainsawStage.name = "chainsaw_runtime_test";
	chainsawStage.arenaRadius = 12.0f;
	chainsawStage.obstacleCount = 1;
	chainsawStage.obstacles[0] = {
		1u,
		{ 6.5f, 0.5f, 0.0f },
		{ 0.5f, 1.0f, 5.0f },
		1u,
		magnet::MagnetObstacleKind::Chainsaw,
	};
	magnet::MagnetChainSystem chainsawSystem;
	if (!chainsawSystem.Initialize(chainsawStage) ||
		!CollectAllBalls(chainsawSystem)) {
		std::cerr << "Chainsaw runtime setup failed.\n";
		return 150;
	}
	magnet::MagnetChainSystem::PlayerCommand obstacleDrive{};
	obstacleDrive.moveDirection = { 1.0f, 0.0f, 0.0f };
	for (int step = 0; step < 180 &&
		chainsawSystem.GetAttachedBallCount() == 8; ++step) {
		chainsawSystem.SetPlayerCommand(obstacleDrive);
		if (!chainsawSystem.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Chainsaw runtime update failed.\n";
			return 151;
		}
	}
	if (chainsawSystem.GetAttachedBallCount() >= 8 ||
		chainsawSystem.GetReleasedBallCount() == 0 ||
		chainsawSystem.GetPhysicsWorld().GetActiveConstraintCount() >= 14) {
		std::cerr << "Chainsaw did not sever and release the contacted outer segment.\n";
		return 152;
	}

	magnet::MagnetStageData furnaceStage = chainsawStage;
	furnaceStage.name = "furnace_runtime_test";
	furnaceStage.obstacles[0].obstacleKind = magnet::MagnetObstacleKind::Furnace;
	magnet::MagnetChainSystem furnaceSystem;
	if (!furnaceSystem.Initialize(furnaceStage) || !CollectAllBalls(furnaceSystem)) {
		std::cerr << "Furnace runtime setup failed.\n";
		return 153;
	}
	for (int step = 0; step < 180 &&
		furnaceSystem.GetAttachedBallCount() == 8; ++step) {
		furnaceSystem.SetPlayerCommand(obstacleDrive);
		if (!furnaceSystem.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Furnace runtime update failed.\n";
			return 154;
		}
	}
	const std::size_t furnaceInactiveCount = furnaceSystem.GetStageBallCount() -
		furnaceSystem.GetAvailableBallCount() - furnaceSystem.GetAttachedBallCount() -
		furnaceSystem.GetReleasedBallCount();
	if (furnaceSystem.GetAttachedBallCount() >= 8 || furnaceInactiveCount == 0 ||
		furnaceSystem.GetPhysicsWorld().GetActiveConstraintCount() >= 14) {
		std::cerr << "Furnace did not dissolve the contacted chain ball.\n";
		return 155;
	}

	magnet::MagnetStageData playerTransferStage = BuildPickupStage();
	playerTransferStage.name = "player_transfer_gate_runtime_test";
	playerTransferStage.arenaRadius = 12.0f;
	playerTransferStage.obstacleCount = 2;
	playerTransferStage.obstacles[0] = {
		1u,
		{ 4.0f, 1.0f, 0.0f },
		{ 0.5f, 2.0f, 5.0f },
		1u,
		magnet::MagnetObstacleKind::TransferGate,
		1u,
	};
	playerTransferStage.obstacles[1] = {
		2u,
		{ 0.0f, 1.0f, 8.0f },
		{ 5.0f, 2.0f, 0.5f },
		1u,
		magnet::MagnetObstacleKind::TransferGate,
		1u,
	};
	magnet::MagnetChainSystem playerTransferSystem;
	if (!playerTransferSystem.Initialize(playerTransferStage) ||
		!CollectAllBalls(playerTransferSystem)) {
		std::cerr << "Player transfer-gate runtime setup failed.\n";
		return 166;
	}
	bool playerTransferred = false;
	for (int step = 0; step < 180 && !playerTransferred; ++step) {
		playerTransferSystem.SetPlayerCommand(obstacleDrive);
		if (!playerTransferSystem.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Player transfer-gate runtime update failed.\n";
			return 167;
		}
		const physics::SphereBody* player =
			playerTransferSystem.GetPhysicsWorld().GetBody(
				playerTransferSystem.GetPlayerBody());
		playerTransferred = player && player->position.z > 7.0f;
	}
	const physics::SphereBody* transferredRuntimePlayer =
		playerTransferSystem.GetPhysicsWorld().GetBody(
			playerTransferSystem.GetPlayerBody());
	if (!playerTransferred || !transferredRuntimePlayer ||
		transferredRuntimePlayer->linearVelocity.z <= 0.0f ||
		playerTransferSystem.GetAttachedBallCount() != 0 ||
		playerTransferSystem.GetReleasedBallCount() !=
			playerTransferStage.ballCount ||
		playerTransferSystem.GetPhysicsWorld().GetActiveConstraintCount() != 0) {
		std::cerr << "Player gate did not release the chains and rotate Player travel.\n";
		return 168;
	}
	for (std::size_t index = 0;
		index < playerTransferSystem.GetStageBallCount();
		++index) {
		const physics::SphereBody* body =
			playerTransferSystem.GetPhysicsWorld().GetBody(
				playerTransferSystem.GetStageBalls()[index]);
		if (!body || body->position.z > 6.0f) {
			std::cerr << "An attached ball followed the Player through the gate.\n";
			return 169;
		}
	}

	magnet::MagnetStageData releasedTransferStage{};
	releasedTransferStage.name = "released_ball_transfer_gate_runtime_test";
	releasedTransferStage.arenaRadius = 12.0f;
	releasedTransferStage.generation.minimumX = -10.0f;
	releasedTransferStage.generation.maximumX = 10.0f;
	releasedTransferStage.generation.minimumZ = -10.0f;
	releasedTransferStage.generation.maximumZ = 10.0f;
	releasedTransferStage.ballCount = 1;
	releasedTransferStage.generation.ballCount = 1;
	releasedTransferStage.balls[0] = { 1u, { 1.05f, 0.5f, 0.0f } };
	releasedTransferStage.obstacleCount = 2;
	releasedTransferStage.obstacles[0] = {
		1u,
		{ 1.25f, 1.0f, 0.0f },
		{ 0.4f, 2.0f, 1.5f },
		1u,
		magnet::MagnetObstacleKind::TransferGate,
		2u,
	};
	releasedTransferStage.obstacles[1] = {
		2u,
		{ 0.0f, 1.0f, 6.0f },
		{ 1.5f, 2.0f, 0.4f },
		1u,
		magnet::MagnetObstacleKind::TransferGate,
		2u,
	};
	magnet::MagnetChainSystem releasedTransferSystem;
	if (!releasedTransferSystem.Initialize(releasedTransferStage) ||
		!releasedTransferSystem.FixedUpdate(kFixedDeltaTime) ||
		releasedTransferSystem.GetAttachedBallCount() != 1) {
		std::cerr << "Released-ball transfer-gate setup failed.\n";
		return 170;
	}
	magnet::MagnetChainSystem::PlayerCommand releaseIntoGate{};
	releaseIntoGate.releaseChains = true;
	releasedTransferSystem.SetPlayerCommand(releaseIntoGate);
	if (!releasedTransferSystem.FixedUpdate(kFixedDeltaTime)) {
		std::cerr << "Released-ball transfer-gate update failed.\n";
		return 171;
	}
	const physics::SphereBody* transferredReleasedBall =
		releasedTransferSystem.GetPhysicsWorld().GetBody(
			releasedTransferSystem.GetStageBalls()[0]);
	const physics::SphereBody* nonTransferredPlayer =
		releasedTransferSystem.GetPhysicsWorld().GetBody(
			releasedTransferSystem.GetPlayerBody());
	if (!transferredReleasedBall || !nonTransferredPlayer ||
		releasedTransferSystem.GetStageBallStates()[0] !=
			magnet::MagnetChainSystem::StageBallState::Released ||
		transferredReleasedBall->position.z < 5.0f ||
		DistanceXZ(nonTransferredPlayer->position, {}) > 0.1f) {
		std::cerr << "A released ball did not transfer independently of the Player.\n";
		return 172;
	}
	std::cout << "obstacle_gameplay_checks=passed\n";

	magnet::MagnetChainSystem goalCleanupSystem;
	if (!goalCleanupSystem.Initialize(BuildPickupStage()) ||
		!CollectAllBalls(goalCleanupSystem)) {
		std::cerr << "Goal connection cleanup setup failed.\n";
		return 130;
	}
	magnet::MagneticImpactAttachmentSystem::Settings goalImpactSettings{};
	goalImpactSettings.minimumImpactSpeed = 0.0f;
	goalImpactSettings.captureMargin = 0.5f;
	goalImpactSettings.releaseGraceSeconds = 0.0f;
	goalCleanupSystem.SetImpactAttachmentSettings(goalImpactSettings);
	magnet::MagnetChainSystem::PlayerCommand goalReleaseCommand{};
	goalReleaseCommand.releaseChains = true;
	goalCleanupSystem.SetPlayerCommand(goalReleaseCommand);
	if (!goalCleanupSystem.FixedUpdate(kFixedDeltaTime) ||
		goalCleanupSystem.GetMagneticAttachmentCount() == 0) {
		std::cerr << "Goal connection cleanup could not create a released-ball joint.\n";
		return 131;
	}
	physics::BodyHandle goalTarget{};
	for (const physics::DistanceConstraint& constraint :
		goalCleanupSystem.GetPhysicsWorld().GetConstraints()) {
		if (constraint.active) {
			goalTarget = constraint.bodyA;
			break;
		}
	}
	const physics::SphereBody* goalTargetBody =
		goalCleanupSystem.GetPhysicsWorld().GetBody(goalTarget);
	if (!goalTargetBody || !goalTargetBody->active) {
		std::cerr << "Goal connection cleanup did not find an active joint endpoint.\n";
		return 132;
	}
	goalCleanupSystem.ConfigureGoal(
		magnet::MagnetChainSystem::GoalSize::Small,
		goalTargetBody->position);
	goalCleanupSystem.SetPlayerCommand({});
	if (!goalCleanupSystem.FixedUpdate(kFixedDeltaTime) ||
		goalCleanupSystem.GetGoalHitCount() == 0) {
		std::cerr << "Goal connection cleanup did not collect the target magnet.\n";
		return 133;
	}
	for (const physics::DistanceConstraint& constraint :
		goalCleanupSystem.GetPhysicsWorld().GetConstraints()) {
		if (!constraint.active) { continue; }
		const physics::SphereBody* bodyA =
			goalCleanupSystem.GetPhysicsWorld().GetBody(constraint.bodyA);
		const physics::SphereBody* bodyB =
			goalCleanupSystem.GetPhysicsWorld().GetBody(constraint.bodyB);
		if (!bodyA || !bodyB || !bodyA->active || !bodyB->active) {
			std::cerr << "A Goal-scored magnet retained an active connection.\n";
			return 134;
		}
	}

	const magnet::MagnetStageData pickupStage = BuildPickupStage();
	magnet::MagnetChainSystem system;
	if (!system.Initialize(pickupStage) || !ValidateFiniteBodies(system)) {
		std::cerr << "Runtime initialization failed.\n";
		return 17;
	}
	if (system.GetAttachedBallCount() != 0 ||
		system.GetAvailableBallCount() != pickupStage.ballCount ||
		system.GetPhysicsWorld().GetBodyCount() !=
			1 + magnet::MagnetChainSystem::kStageBallCapacity ||
		system.GetPhysicsWorld().GetConstraintCount() != 14 ||
		system.GetPhysicsWorld().GetActiveConstraintCount() != 0) {
		std::cerr << "Chains did not start empty with reusable inactive constraints.\n";
		return 18;
	}
	if (!CollectAllBalls(system)) {
		std::cerr << "Loose balls did not attach to balanced left/right chains.\n";
		return 19;
	}

	magnet::MagnetChainSystem::PlayerCommand command{};
	command.moveDirection = { 0.0f, 0.0f, 1.0f };
	for (int step = 0; step < kDriveStepCount; ++step) {
		system.SetPlayerCommand(command);
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			std::cerr << "Drive simulation failed.\n";
			return 13;
		}
	}

	command = {};
	command.emergencyStop = true;
	float maximumInnerForwardOffset = 0.0f;
	float maximumOuterForwardOffset = 0.0f;
	float maximumConstraintError = 0.0f;
	for (int step = 0; step < kStopObservationStepCount; ++step) {
		system.SetPlayerCommand(command);
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			std::cerr << "Stop simulation failed.\n";
			return 14;
		}
		command.emergencyStop = false;
		const physics::PhysicsWorld& world = system.GetPhysicsWorld();
		const physics::SphereBody* player = world.GetBody(system.GetPlayerBody());
		const physics::SphereBody* inner = world.GetBody(system.GetRightChain().front());
		const physics::SphereBody* outer = world.GetBody(
			system.GetRightChain()[system.GetRightChainCount() - 1]);
		if (!player || !inner || !outer) {
			std::cerr << "Attached body handle became stale.\n";
			return 15;
		}
		maximumInnerForwardOffset = (std::max)(
			maximumInnerForwardOffset,
			inner->position.z - player->position.z);
		maximumOuterForwardOffset = (std::max)(
			maximumOuterForwardOffset,
			outer->position.z - player->position.z);
		maximumConstraintError = (std::max)(
			maximumConstraintError,
			system.GetMaximumConstraintError());
	}
	std::cout << "inner_forward_offset=" << maximumInnerForwardOffset << '\n';
	std::cout << "outer_forward_offset=" << maximumOuterForwardOffset << '\n';
	std::cout << "maximum_constraint_error=" << maximumConstraintError << '\n';
	if (!(maximumOuterForwardOffset > maximumInnerForwardOffset) ||
		maximumOuterForwardOffset <= 0.0f ||
		!std::isfinite(maximumConstraintError) ||
		maximumConstraintError > kMaximumAllowedConstraintError) {
		std::cerr << "Collected chain lost its bounded elastic response.\n";
		return 16;
	}

	for (int step = 0; step < 180; ++step) {
		command = {};
		command.moveDirection = ((step / 30) % 2 == 0)
			? Vector3{ 1.0f, 0.0f, 0.0f }
			: Vector3{ -1.0f, 0.0f, 0.0f };
		system.SetPlayerCommand(command);
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			std::cerr << "Pre-release swing simulation failed.\n";
			return 17;
		}
	}

	command = {};
	command.emergencyStop = true;
	for (int step = 0; step < 3; ++step) {
		system.SetPlayerCommand(command);
		if (!system.FixedUpdate(kFixedDeltaTime)) {
			std::cerr << "Release setup failed.\n";
			return 18;
		}
		command.emergencyStop = false;
	}
	const physics::SphereBody* playerBeforeRelease =
		system.GetPhysicsWorld().GetBody(system.GetPlayerBody());
	if (!playerBeforeRelease) {
		return 19;
	}
	const Vector3 playerReleasePosition = playerBeforeRelease->position;
	std::array<Vector3, 8> releasePositions{};
	for (std::size_t index = 0; index < pickupStage.ballCount; ++index) {
		const physics::SphereBody* body = system.GetPhysicsWorld().GetBody(
			system.GetStageBalls()[index]);
		if (!body) {
			return 20;
		}
		releasePositions[index] = body->position;
	}
	command = {};
	command.emergencyStop = true;
	command.releaseChains = true;
	system.SetPlayerCommand(command);
	if (!system.FixedUpdate(kFixedDeltaTime) || system.HasAttachedBalls() ||
		system.GetReleasedBallCount() != pickupStage.ballCount ||
		system.GetPhysicsWorld().GetActiveConstraintCount() != 0) {
		std::cerr << "Release did not detach only the collected balls.\n";
		return 21;
	}
	bool observedReleaseLift = false;
	for (std::size_t index = 0; index < pickupStage.ballCount; ++index) {
		const physics::SphereBody* releasedBody = system.GetPhysicsWorld().GetBody(
			system.GetStageBalls()[index]);
		if (!releasedBody || releasedBody->lockToHorizontalPlane ||
			releasedBody->position.y < releasedBody->radius - 1.0e-4f) {
			std::cerr << "Released ball did not enter bounded 3D ground motion.\n";
			return 140;
		}
		observedReleaseLift = observedReleaseLift ||
			releasedBody->linearVelocity.y > 0.1f;
	}
	if (!observedReleaseLift) {
		std::cerr << "Moving released balls did not receive launch lift.\n";
		return 141;
	}
	const auto& diagnostics = system.GetLastReleaseConvergenceDiagnostics();
	if (!diagnostics.valid || !IsFinite(diagnostics.focusPoint) ||
		!std::isfinite(diagnostics.predictedRmsSpreadBefore) ||
		!std::isfinite(diagnostics.predictedRmsSpreadAfter) ||
		diagnostics.predictedRmsSpreadAfter > diagnostics.predictedRmsSpreadBefore ||
		diagnostics.maximumDirectionCorrectionRadians > kMaximumReleaseCorrectionRadians) {
		std::cerr << "Release convergence diagnostics are invalid.\n";
		return 22;
	}

	command = {};
	for (int step = 0; step < kReleaseTravelStepCount; ++step) {
		system.SetPlayerCommand(command);
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			std::cerr << "Released-ball simulation failed.\n";
			return 23;
		}
	}
	const physics::SphereBody* playerAfterRelease =
		system.GetPhysicsWorld().GetBody(system.GetPlayerBody());
	if (!playerAfterRelease ||
		DistanceXZ(playerReleasePosition, playerAfterRelease->position) > 1.0e-4f ||
		DistanceXZ({}, playerAfterRelease->linearVelocity) > 1.0e-4f) {
		std::cerr << "Player moved because of chain release.\n";
		return 24;
	}
	float rootTravel = 0.0f;
	float totalTravel = 0.0f;
	for (std::size_t index = 0; index < pickupStage.ballCount; ++index) {
		const physics::SphereBody* body = system.GetPhysicsWorld().GetBody(
			system.GetStageBalls()[index]);
		if (!body) {
			return 25;
		}
		const float travel = DistanceXZ(releasePositions[index], body->position);
		totalTravel += travel;
		if (index < 2) {
			rootTravel += travel;
		}
	}
	std::cout << "released_total_travel=" << totalTravel << '\n';
	std::cout << "released_root_sample_travel=" << rootTravel << '\n';
	if (totalTravel <= 0.25f || rootTravel < kMinimumRootReleaseTravel) {
		std::cerr << "Collected balls lost meaningful release inertia.\n";
		return 26;
	}

	if (!system.Reset() || system.GetAttachedBallCount() != 0 ||
		system.GetAvailableBallCount() != pickupStage.ballCount ||
		system.GetLastReleaseConvergenceDiagnostics().valid) {
		std::cerr << "Reset did not restore the authored loose-ball stage.\n";
		return 27;
	}

	std::cout << "MAGNET_STAGE_AND_CHAIN_TESTS_OK\n";
	return 0;
}
