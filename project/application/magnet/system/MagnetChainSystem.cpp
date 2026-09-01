#include "application/magnet/system/MagnetChainSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

constexpr float kPlayerRadius = 0.75f;
constexpr float kMagnetRadius = 0.5f;
constexpr float kFirstLinkLength = kPlayerRadius + kMagnetRadius;
constexpr float kFirstHorizontalOffset = 1.2247449f; // sqrt(firstLink^2 - heightDifference^2)
constexpr float kLinkLength = kMagnetRadius * 2.0f;
constexpr float kPlaneHeight = kPlayerRadius;
constexpr float kPlayerMaximumSpeed = 6.0f;
constexpr float kPlayerAcceleration = 12.0f;
constexpr float kPlayerDeceleration = 9.0f;
constexpr float kDirectionEpsilonSquared = 1.0e-8f;
constexpr float kDynamicBodyMass = 1.0f;
constexpr float kDynamicBodyDamping = 0.35f;
constexpr float kConstraintCompliance = 0.00002f;
constexpr float kMaximumConstraintCorrection = 0.35f;
constexpr float kTestBallRadius = 0.3f;
constexpr float kTestBallSpawnForwardOffset = 1.8f;
constexpr float kTestBallMaximumDistance = 32.0f;
constexpr float kEmitterMinimumInterval = 0.05f;
constexpr float kEmitterMaximumInterval = 2.0f;
constexpr float kEmitterMinimumSpeed = 1.0f;
constexpr float kEmitterMaximumSpeed = 20.0f;
constexpr uint32_t kPhysicsSubsteps = 3;
constexpr uint32_t kConstraintIterations = 8;

bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float LengthSquaredXZ(const Vector3& value) noexcept
{
	return value.x * value.x + value.z * value.z;
}

Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maximumDelta) noexcept
{
	const Vector3 delta = target - current;
	const float distanceSquared = LengthSquaredXZ(delta);
	if (distanceSquared <= maximumDelta * maximumDelta || distanceSquared <= kDirectionEpsilonSquared) {
		return target;
	}
	const float distance = std::sqrt(distanceSquared);
	return current + delta * (maximumDelta / distance);
}

} // namespace

bool MagnetChainSystem::Initialize()
{
	return Reset();
}

bool MagnetChainSystem::Reset()
{
	physicsWorld_.Clear();
	playerVelocity_ = {};
	command_ = {};
	emitterTimer_ = 0.0f;
	nextTestBallIndex_ = 0;
	emittedBallSequence_ = 0;
	healthy_ = false;

	physics::SphereBodyDesc playerDesc{};
	playerDesc.position = { 0.0f, kPlaneHeight, 0.0f };
	playerDesc.radius = kPlayerRadius;
	playerDesc.planeHeight = kPlaneHeight;
	playerDesc.motionType = physics::MotionType::Kinematic;
	playerBody_ = physicsWorld_.CreateSphereBody(playerDesc);
	if (!playerBody_.IsValid()) {
		return false;
	}

	if (!CreateChain(-1.0f, leftChain_) || !CreateChain(1.0f, rightChain_) ||
		!CreateTestBallPool()) {
		return false;
	}

	healthy_ = true;
	return true;
}

void MagnetChainSystem::SetEmitterSettings(const EmitterSettings& settings) noexcept
{
	emitterSettings_.autoEmit = settings.autoEmit;
	emitterSettings_.intervalSeconds = std::isfinite(settings.intervalSeconds)
		? std::clamp(settings.intervalSeconds, kEmitterMinimumInterval, kEmitterMaximumInterval)
		: 0.35f;
	emitterSettings_.launchSpeed = std::isfinite(settings.launchSpeed)
		? std::clamp(settings.launchSpeed, kEmitterMinimumSpeed, kEmitterMaximumSpeed)
		: 8.0f;
	if (!emitterSettings_.autoEmit) {
		emitterTimer_ = 0.0f;
	}
}

bool MagnetChainSystem::FixedUpdate(float fixedDeltaTime) noexcept
{
	if (!healthy_ || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f ||
		!IsFinite(command_.moveDirection)) {
		return false;
	}

	Vector3 requestedDirection = command_.moveDirection;
	requestedDirection.y = 0.0f;
	const float directionLengthSquared = LengthSquaredXZ(requestedDirection);
	Vector3 targetVelocity{};
	if (directionLengthSquared > kDirectionEpsilonSquared) {
		const float inverseLength = 1.0f / std::sqrt(directionLengthSquared);
		targetVelocity = requestedDirection * (kPlayerMaximumSpeed * inverseLength);
	}

	if (command_.emergencyStop) {
		playerVelocity_ = {};
	} else {
		const float acceleration = directionLengthSquared > kDirectionEpsilonSquared
			? kPlayerAcceleration
			: kPlayerDeceleration;
		playerVelocity_ = MoveTowards(
			playerVelocity_, targetVelocity, acceleration * fixedDeltaTime);
	}

	if (!physicsWorld_.SetLinearVelocity(playerBody_, playerVelocity_) ||
		(command_.emitOne && !EmitTestBall())) {
		healthy_ = false;
		return false;
	}

	if (emitterSettings_.autoEmit) {
		emitterTimer_ += fixedDeltaTime;
		if (emitterTimer_ >= emitterSettings_.intervalSeconds) {
			emitterTimer_ = std::fmod(emitterTimer_, emitterSettings_.intervalSeconds);
			if (!EmitTestBall()) {
				healthy_ = false;
				return false;
			}
		}
	}

	if (!physicsWorld_.Step(fixedDeltaTime, kPhysicsSubsteps, kConstraintIterations)) {
		healthy_ = false;
		return false;
	}
	DeactivateDistantTestBalls();
	return true;
}

bool MagnetChainSystem::CreateChain(
	float sideSign,
	std::array<physics::BodyHandle, kLinksPerSide>& outputChain)
{
	physics::BodyHandle previousBody = playerBody_;
	float distanceFromPlayer = kFirstHorizontalOffset;
	for (std::size_t linkIndex = 0; linkIndex < outputChain.size(); ++linkIndex) {
		physics::SphereBodyDesc bodyDesc{};
		bodyDesc.position = { sideSign * distanceFromPlayer, kMagnetRadius, 0.0f };
		bodyDesc.radius = kMagnetRadius;
		bodyDesc.mass = kDynamicBodyMass;
		bodyDesc.linearDamping = kDynamicBodyDamping;
		bodyDesc.planeHeight = kMagnetRadius;
		bodyDesc.motionType = physics::MotionType::Dynamic;
		outputChain[linkIndex] = physicsWorld_.CreateSphereBody(bodyDesc);
		if (!outputChain[linkIndex].IsValid()) {
			return false;
		}

		physics::DistanceConstraintDesc constraintDesc{};
		constraintDesc.bodyA = previousBody;
		constraintDesc.bodyB = outputChain[linkIndex];
		constraintDesc.restLength = linkIndex == 0 ? kFirstLinkLength : kLinkLength;
		constraintDesc.compliance = kConstraintCompliance;
		constraintDesc.maximumCorrection = kMaximumConstraintCorrection;
		if (!physicsWorld_.CreateDistanceConstraint(constraintDesc)) {
			return false;
		}

		previousBody = outputChain[linkIndex];
		distanceFromPlayer += kLinkLength;
	}
	return true;
}

bool MagnetChainSystem::CreateTestBallPool()
{
	for (physics::BodyHandle& handle : testBalls_) {
		physics::SphereBodyDesc bodyDesc{};
		bodyDesc.position = { 0.0f, kTestBallRadius, 0.0f };
		bodyDesc.radius = kTestBallRadius;
		bodyDesc.mass = 0.5f;
		bodyDesc.linearDamping = 0.0f;
		bodyDesc.planeHeight = kTestBallRadius;
		bodyDesc.motionType = physics::MotionType::Dynamic;
		bodyDesc.active = false;
		handle = physicsWorld_.CreateSphereBody(bodyDesc);
		if (!handle.IsValid()) {
			return false;
		}
	}
	return true;
}

bool MagnetChainSystem::EmitTestBall() noexcept
{
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player || testBalls_.empty()) {
		return false;
	}

	const physics::BodyHandle handle = testBalls_[nextTestBallIndex_];
	nextTestBallIndex_ = (nextTestBallIndex_ + 1) % testBalls_.size();
	const int spreadSlot = static_cast<int>(emittedBallSequence_ % 5u) - 2;
	++emittedBallSequence_;
	Vector3 direction = { static_cast<float>(spreadSlot) * 0.10f, 0.0f, 1.0f };
	const float inverseDirectionLength =
		1.0f / std::sqrt(direction.x * direction.x + direction.z * direction.z);
	direction *= inverseDirectionLength;
	const Vector3 spawnPosition = {
		player->position.x,
		kTestBallRadius,
		player->position.z + kTestBallSpawnForwardOffset,
	};

	return physicsWorld_.SetActive(handle, false) &&
		physicsWorld_.SetPosition(handle, spawnPosition) &&
		physicsWorld_.SetActive(handle, true) &&
		physicsWorld_.SetLinearVelocity(handle, direction * emitterSettings_.launchSpeed);
}

void MagnetChainSystem::DeactivateDistantTestBalls() noexcept
{
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player) {
		return;
	}
	const float maximumDistanceSquared = kTestBallMaximumDistance * kTestBallMaximumDistance;
	for (physics::BodyHandle handle : testBalls_) {
		const physics::SphereBody* body = physicsWorld_.GetBody(handle);
		if (!body || !body->active) {
			continue;
		}
		const Vector3 delta = body->position - player->position;
		if (delta.x * delta.x + delta.z * delta.z > maximumDistanceSquared) {
			(void)physicsWorld_.SetActive(handle, false);
		}
	}
}

std::size_t MagnetChainSystem::GetActiveTestBallCount() const noexcept
{
	std::size_t activeCount = 0;
	for (physics::BodyHandle handle : testBalls_) {
		const physics::SphereBody* body = physicsWorld_.GetBody(handle);
		if (body && body->active) {
			++activeCount;
		}
	}
	return activeCount;
}

float MagnetChainSystem::GetMaximumConstraintError() const noexcept
{
	float maximumError = 0.0f;
	for (const physics::DistanceConstraint& constraint : physicsWorld_.GetConstraints()) {
		const physics::SphereBody* bodyA = physicsWorld_.GetBody(constraint.bodyA);
		const physics::SphereBody* bodyB = physicsWorld_.GetBody(constraint.bodyB);
		if (!bodyA || !bodyB || !bodyA->active || !bodyB->active) {
			continue;
		}
		const Vector3 delta = bodyB->position - bodyA->position;
		const float distance = std::sqrt(
			delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		if (!std::isfinite(distance)) {
			return INFINITY;
		}
		maximumError = (std::max)(
			maximumError, std::abs(distance - constraint.restLength));
	}
	return maximumError;
}

} // namespace magnet
