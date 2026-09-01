#include "application/magnet/system/MagnetChainSystem.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

constexpr float kFixedDeltaTime = 1.0f / 60.0f;
constexpr int kDriveStepCount = 120;
constexpr int kStopObservationStepCount = 180;
constexpr float kMaximumAllowedConstraintError = 0.08f;

bool IsFinite(const Vector3& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float Distance(const Vector3& a, const Vector3& b)
{
	const Vector3 delta = b - a;
	return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

bool ValidateFiniteBodies(const magnet::MagnetChainSystem& system)
{
	const physics::PhysicsWorld& world = system.GetPhysicsWorld();
	const physics::SphereBody* player = world.GetBody(system.GetPlayerBody());
	if (!player || !IsFinite(player->position) || !IsFinite(player->linearVelocity)) {
		return false;
	}
	for (physics::BodyHandle handle : system.GetLeftChain()) {
		const physics::SphereBody* body = world.GetBody(handle);
		if (!body || !IsFinite(body->position) || !IsFinite(body->linearVelocity)) {
			return false;
		}
	}
	for (physics::BodyHandle handle : system.GetRightChain()) {
		const physics::SphereBody* body = world.GetBody(handle);
		if (!body || !IsFinite(body->position) || !IsFinite(body->linearVelocity)) {
			return false;
		}
	}
	for (physics::BodyHandle handle : system.GetTestBalls()) {
		const physics::SphereBody* body = world.GetBody(handle);
		if (!body || !IsFinite(body->position) || !IsFinite(body->linearVelocity)) {
			return false;
		}
	}
	return true;
}

float GetMaximumConstraintError(const magnet::MagnetChainSystem& system)
{
	const physics::PhysicsWorld& world = system.GetPhysicsWorld();
	float maximumError = 0.0f;
	for (const physics::DistanceConstraint& constraint : world.GetConstraints()) {
		const physics::SphereBody* bodyA = world.GetBody(constraint.bodyA);
		const physics::SphereBody* bodyB = world.GetBody(constraint.bodyB);
		if (!bodyA || !bodyB) {
			return INFINITY;
		}
		maximumError = (std::max)(
			maximumError,
			std::abs(Distance(bodyA->position, bodyB->position) - constraint.restLength));
	}
	return maximumError;
}

} // namespace

int main()
{
	magnet::MagnetChainSystem system;
	if (!system.Initialize()) {
		std::cerr << "Initialize failed.\n";
		return 1;
	}
	if (system.GetPhysicsWorld().GetBodyCount() !=
			1 + magnet::MagnetChainSystem::kLinksPerSide * 2 +
			magnet::MagnetChainSystem::kTestBallCapacity ||
		system.GetPhysicsWorld().GetConstraints().size() != 8) {
		std::cerr << "Unexpected prototype topology.\n";
		return 2;
	}
	magnet::MagnetChainSystem::EmitterSettings emitterSettings{};
	emitterSettings.autoEmit = true;
	emitterSettings.intervalSeconds = 0.05f;
	emitterSettings.launchSpeed = 20.0f;
	system.SetEmitterSettings(emitterSettings);
	for (int step = 0; step < 120; ++step) {
		system.SetPlayerCommand({});
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			std::cerr << "Emitter simulation became invalid at step " << step << ".\n";
			return 10;
		}
	}
	if (system.GetActiveTestBallCount() == 0 ||
		system.GetActiveTestBallCount() > magnet::MagnetChainSystem::kTestBallCapacity) {
		std::cerr << "Emitter Pool count is invalid.\n";
		return 11;
	}
	if (!system.Reset()) {
		std::cerr << "Reset after emitter test failed.\n";
		return 12;
	}

	magnet::MagnetChainSystem::PlayerCommand command{};
	command.moveDirection = { 0.0f, 0.0f, 1.0f };
	for (int step = 0; step < kDriveStepCount; ++step) {
		system.SetPlayerCommand(command);
		if (!system.FixedUpdate(kFixedDeltaTime) || !ValidateFiniteBodies(system)) {
			std::cerr << "Drive simulation became invalid at step " << step << ".\n";
			return 3;
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
			std::cerr << "Stop simulation became invalid at step " << step << ".\n";
			return 4;
		}
		command.emergencyStop = false;

		const physics::PhysicsWorld& world = system.GetPhysicsWorld();
		const physics::SphereBody* player = world.GetBody(system.GetPlayerBody());
		const physics::SphereBody* inner = world.GetBody(system.GetRightChain().front());
		const physics::SphereBody* outer = world.GetBody(system.GetRightChain().back());
		if (!player || !inner || !outer) {
			std::cerr << "Body handle became stale.\n";
			return 5;
		}
		maximumInnerForwardOffset = (std::max)(
			maximumInnerForwardOffset, inner->position.z - player->position.z);
		maximumOuterForwardOffset = (std::max)(
			maximumOuterForwardOffset, outer->position.z - player->position.z);
		maximumConstraintError = (std::max)(maximumConstraintError, GetMaximumConstraintError(system));
	}

	std::cout << "inner_forward_offset=" << maximumInnerForwardOffset << '\n';
	std::cout << "outer_forward_offset=" << maximumOuterForwardOffset << '\n';
	std::cout << "maximum_constraint_error=" << maximumConstraintError << '\n';
	if (!(maximumOuterForwardOffset > maximumInnerForwardOffset) ||
		maximumOuterForwardOffset <= 0.0f) {
		std::cerr << "Outer link did not produce the expected stronger forward reaction.\n";
		return 6;
	}
	if (!std::isfinite(maximumConstraintError) ||
		maximumConstraintError > kMaximumAllowedConstraintError) {
		std::cerr << "Constraint error exceeded the prototype bound.\n";
		return 7;
	}

	if (!system.Reset() || !ValidateFiniteBodies(system)) {
		std::cerr << "Reset failed.\n";
		return 8;
	}
	const physics::SphereBody* resetPlayer =
		system.GetPhysicsWorld().GetBody(system.GetPlayerBody());
	if (!resetPlayer || std::abs(resetPlayer->position.x) > 1.0e-5f ||
		std::abs(resetPlayer->position.z) > 1.0e-5f) {
		std::cerr << "Reset did not restore the player origin.\n";
		return 9;
	}

	std::cout << "MAGNET_CHAIN_PHYSICS_TESTS_OK\n";
	return 0;
}
