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
constexpr float kPlayerTurnRateRadians = 4.5f;
constexpr float kDirectionEpsilonSquared = 1.0e-8f;
constexpr float kDynamicBodyMass = 1.0f;
constexpr float kDynamicBodyDamping = 0.12f;
constexpr float kConstraintCompliance = 0.00008f;
constexpr float kMaximumConstraintCorrection = 0.35f;
constexpr float kMaximumMagneticAcceleration = 42.0f;
constexpr float kMaximumMagneticVelocityChange = 1.25f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr std::array<float, MagnetChainSystem::kLinksPerSide> kMagneticStiffness = {
	24.0f, 14.0f, 8.0f, 4.5f,
};
constexpr std::array<float, MagnetChainSystem::kLinksPerSide> kMagneticDamping = {
	4.2f, 3.0f, 2.0f, 1.2f,
};
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

float MoveAngleTowards(float current, float target, float maximumDelta) noexcept
{
	const float delta = std::remainder(target - current, kTwoPi);
	if (std::abs(delta) <= maximumDelta) {
		return current + delta;
	}
	return current + std::copysign(maximumDelta, delta);
}

Vector3 ClampMagnitudeXZ(const Vector3& value, float maximumMagnitude) noexcept
{
	const float lengthSquared = LengthSquaredXZ(value);
	const float maximumMagnitudeSquared = maximumMagnitude * maximumMagnitude;
	if (lengthSquared <= maximumMagnitudeSquared || lengthSquared <= kDirectionEpsilonSquared) {
		return value;
	}
	return value * (maximumMagnitude / std::sqrt(lengthSquared));
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
	playerHeadingRadians_ = 0.0f;
	command_ = {};
	emitterTimer_ = 0.0f;
	nextTestBallIndex_ = 0;
	emittedBallSequence_ = 0;
	leftConstraintIndices_.fill(kInvalidConstraintIndex);
	rightConstraintIndices_.fill(kInvalidConstraintIndex);
	chainsAttached_ = true;
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

	if (!CreateChain(-1.0f, leftChain_, leftConstraintIndices_) ||
		!CreateChain(1.0f, rightChain_, rightConstraintIndices_) ||
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
	if (directionLengthSquared > kDirectionEpsilonSquared) {
		const float targetHeading = std::atan2(requestedDirection.x, requestedDirection.z);
		playerHeadingRadians_ = MoveAngleTowards(
			playerHeadingRadians_, targetHeading, kPlayerTurnRateRadians * fixedDeltaTime);
	}

	if (!physicsWorld_.SetLinearVelocity(playerBody_, playerVelocity_) ||
		(chainsAttached_ && !ApplyMagneticRestoringForces(fixedDeltaTime)) ||
		(command_.releaseChains && chainsAttached_ && !ReleaseChains()) ||
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
	std::array<physics::BodyHandle, kLinksPerSide>& outputChain,
	std::array<std::size_t, kLinksPerSide>& outputConstraintIndices)
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
		outputConstraintIndices[linkIndex] = physicsWorld_.GetConstraintCount();
		if (!physicsWorld_.CreateDistanceConstraint(constraintDesc)) {
			outputConstraintIndices[linkIndex] = kInvalidConstraintIndex;
			return false;
		}

		previousBody = outputChain[linkIndex];
		distanceFromPlayer += kLinkLength;
	}
	return true;
}

bool MagnetChainSystem::ApplyMagneticRestoringForces(float fixedDeltaTime) noexcept
{
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player || !player->active) {
		return false;
	}

	const Vector3 rightAxis = {
		std::cos(playerHeadingRadians_),
		0.0f,
		-std::sin(playerHeadingRadians_),
	};
	const auto applyToChain = [&](float sideSign, const auto& chain) noexcept {
		for (std::size_t linkIndex = 0; linkIndex < chain.size(); ++linkIndex) {
			const physics::SphereBody* body = physicsWorld_.GetBody(chain[linkIndex]);
			if (!body || !body->active || body->motionType != physics::MotionType::Dynamic) {
				return false;
			}
			const float horizontalDistance =
				kFirstHorizontalOffset + static_cast<float>(linkIndex) * kLinkLength;
			Vector3 targetPosition =
				player->position + rightAxis * (sideSign * horizontalDistance);
			targetPosition.y = body->planeHeight;
			Vector3 positionError = targetPosition - body->position;
			positionError.y = 0.0f;
			Vector3 relativeVelocity = body->linearVelocity - playerVelocity_;
			relativeVelocity.y = 0.0f;
			Vector3 acceleration =
				positionError * kMagneticStiffness[linkIndex] -
				relativeVelocity * kMagneticDamping[linkIndex];
			acceleration = ClampMagnitudeXZ(acceleration, kMaximumMagneticAcceleration);
			const Vector3 velocityChange = ClampMagnitudeXZ(
				acceleration * fixedDeltaTime,
				kMaximumMagneticVelocityChange);
			if (!IsFinite(velocityChange) ||
				!physicsWorld_.SetLinearVelocity(
					chain[linkIndex], body->linearVelocity + velocityChange)) {
				return false;
			}
		}
		return true;
	};

	return applyToChain(-1.0f, leftChain_) && applyToChain(1.0f, rightChain_);
}

bool MagnetChainSystem::ReleaseChains() noexcept
{
	const std::size_t constraintCount = physicsWorld_.GetConstraintCount();
	for (std::size_t index : leftConstraintIndices_) {
		if (index == kInvalidConstraintIndex || index >= constraintCount) {
			return false;
		}
	}
	for (std::size_t index : rightConstraintIndices_) {
		if (index == kInvalidConstraintIndex || index >= constraintCount) {
			return false;
		}
	}

	for (std::size_t index : leftConstraintIndices_) {
		if (!physicsWorld_.SetDistanceConstraintActive(index, false)) {
			return false;
		}
	}
	for (std::size_t index : rightConstraintIndices_) {
		if (!physicsWorld_.SetDistanceConstraintActive(index, false)) {
			return false;
		}
	}
	chainsAttached_ = false;
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
		if (!constraint.active) {
			continue;
		}
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
