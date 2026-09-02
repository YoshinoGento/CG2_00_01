#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/BallMomentumTracker.h"
#include "application/magnet/system/MagnetChainSystem.h"

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

float DistanceXZ(const Vector3& a, const Vector3& b)
{
	const float deltaX = b.x - a.x;
	const float deltaZ = b.z - a.z;
	return std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
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
	if (!player || !IsFinite(player->position) || !IsFinite(player->linearVelocity)) {
		return false;
	}
	for (std::size_t index = 0; index < system.GetStageBallCount(); ++index) {
		const physics::SphereBody* body = world.GetBody(system.GetStageBalls()[index]);
		if (!body || !IsFinite(body->position) || !IsFinite(body->linearVelocity)) {
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
	magnet::MagnetStageSystem defaultStageSystem;
	if (!defaultStageSystem.Load("project/Resources/levels/magnet/stage_01.json") ||
		defaultStageSystem.GetStageData().ballCount != 16) {
		std::cerr << "Tracked default stage JSON is invalid.\n";
		return 7;
	}

	const std::string roundTripPath = "generated/tests/magnet_stage_roundtrip.json";
	if (!stageSystem.Save(roundTripPath)) {
		std::cerr << "Stage JSON save failed.\n";
		return 8;
	}
	magnet::MagnetStageSystem loadedStageSystem;
	if (!loadedStageSystem.Load(roundTripPath) ||
		loadedStageSystem.GetStageData().ballCount != generated.ballCount) {
		std::cerr << "Stage JSON load failed.\n";
		return 9;
	}
	std::error_code removeError;
	std::filesystem::remove(roundTripPath, removeError);
	if (removeError) {
		std::cerr << "Stage round-trip cleanup failed.\n";
		return 10;
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
