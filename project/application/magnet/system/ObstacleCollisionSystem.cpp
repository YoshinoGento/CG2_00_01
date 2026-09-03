#include "application/magnet/system/ObstacleCollisionSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

constexpr float kContactOffset = 0.002f;
constexpr float kEpsilon = 1.0e-6f;
constexpr float kAnchorCaptureDistance = 0.16f;
constexpr float kAnchorAdditionalReach = 3.5f;

Vector3 GetTransferNormal(const MagnetStageBoxPlacement& gate) noexcept
{
	return gate.size.x <= gate.size.z
		? Vector3{ 1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 0.0f, 1.0f };
}

Vector3 GetTransferTangent(const Vector3& normal) noexcept
{
	return { -normal.z, 0.0f, normal.x };
}

float GetHalfExtentAlong(
	const MagnetStageBoxPlacement& gate,
	const Vector3& axis) noexcept
{
	return std::abs(axis.x) > 0.5f
		? gate.size.x * 0.5f
		: gate.size.z * 0.5f;
}

bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float DotXZ(const Vector3& left, const Vector3& right) noexcept
{
	return left.x * right.x + left.z * right.z;
}

float LengthSquaredXZ(const Vector3& value) noexcept
{
	return DotXZ(value, value);
}

bool IsSameBody(physics::BodyHandle left, physics::BodyHandle right) noexcept
{
	return left.index == right.index && left.generation == right.generation;
}

Vector3 ClampMagnitudeXZ(const Vector3& value, float maximumMagnitude) noexcept
{
	const float lengthSquared = LengthSquaredXZ(value);
	if (!std::isfinite(lengthSquared) || lengthSquared <= kEpsilon ||
		lengthSquared <= maximumMagnitude * maximumMagnitude) {
		return value;
	}
	return value * (maximumMagnitude / std::sqrt(lengthSquared));
}

bool SweepPointAgainstBoxXZ(const Vector3& start, const Vector3& end,
	float minimumX, float maximumX, float minimumZ, float maximumZ,
	float& hitTime, Vector3& hitNormal) noexcept
{
	const Vector3 delta = end - start;
	float entry = 0.0f;
	float exit = 1.0f;
	hitNormal = {};
	const auto testAxis = [&](float origin, float direction, float minimum,
		float maximum, const Vector3& negativeNormal,
		const Vector3& positiveNormal) noexcept {
		if (std::abs(direction) <= kEpsilon) {
			return origin >= minimum && origin <= maximum;
		}
		float nearTime = (minimum - origin) / direction;
		float farTime = (maximum - origin) / direction;
		Vector3 nearNormal = negativeNormal;
		if (nearTime > farTime) {
			std::swap(nearTime, farTime);
			nearNormal = positiveNormal;
		}
		if (nearTime > entry) {
			entry = nearTime;
			hitNormal = nearNormal;
		}
		exit = (std::min)(exit, farTime);
		return entry <= exit;
	};
	if (!testAxis(start.x, delta.x, minimumX, maximumX,
		{ -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }) ||
		!testAxis(start.z, delta.z, minimumZ, maximumZ,
		{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }) ||
		entry < 0.0f || entry > 1.0f) {
		return false;
	}
	hitTime = entry;
	return std::abs(hitNormal.x) > 0.0f || std::abs(hitNormal.z) > 0.0f;
}

bool HasVerticalOverlap(const physics::SphereBody& body,
	const MagnetStageBoxPlacement& obstacle) noexcept
{
	const float bottom = obstacle.position.y - obstacle.size.y * 0.5f;
	const float top = obstacle.position.y + obstacle.size.y * 0.5f;
	return body.position.y + body.radius >= bottom &&
		body.position.y - body.radius <= top;
}

} // namespace

void ObstacleCollisionSystem::Reset() noexcept
{
	events_.fill({});
	anchors_.fill({});
	transferCooldowns_.fill(0.0f);
	eventCount_ = 0;
	elapsedSeconds_ = 0.0f;
}

void ObstacleCollisionSystem::SetSettings(const Settings& settings) noexcept
{
	settings_.solidRestitution = std::isfinite(settings.solidRestitution)
		? std::clamp(settings.solidRestitution, 0.0f, 1.0f) : 0.55f;
	settings_.solidTangentialDamping = std::isfinite(settings.solidTangentialDamping)
		? std::clamp(settings.solidTangentialDamping, 0.0f, 1.0f) : 0.88f;
	settings_.bumperRestitution = std::isfinite(settings.bumperRestitution)
		? std::clamp(settings.bumperRestitution, 1.0f, 2.5f) : 1.45f;
	settings_.bumperMaximumSpeed = std::isfinite(settings.bumperMaximumSpeed)
		? std::clamp(settings.bumperMaximumSpeed, 1.0f, 80.0f) : 30.0f;
	settings_.bumperMinimumExitSpeed = std::isfinite(settings.bumperMinimumExitSpeed)
		? std::clamp(settings.bumperMinimumExitSpeed, 0.0f,
			settings_.bumperMaximumSpeed) : 8.5f;
	settings_.anchorHoldSeconds = std::isfinite(settings.anchorHoldSeconds)
		? std::clamp(settings.anchorHoldSeconds, 0.1f, 5.0f) : 1.0f;
	settings_.anchorAttractTimeoutSeconds =
		std::isfinite(settings.anchorAttractTimeoutSeconds)
		? std::clamp(settings.anchorAttractTimeoutSeconds, 0.2f, 8.0f) : 1.75f;
	settings_.anchorCooldownSeconds = std::isfinite(settings.anchorCooldownSeconds)
		? std::clamp(settings.anchorCooldownSeconds, 0.0f, 5.0f) : 0.75f;
	settings_.anchorAcceleration = std::isfinite(settings.anchorAcceleration)
		? std::clamp(settings.anchorAcceleration, 0.0f, 240.0f) : 90.0f;
	settings_.anchorDamping = std::isfinite(settings.anchorDamping)
		? std::clamp(settings.anchorDamping, 0.0f, 40.0f) : 8.5f;
	settings_.anchorMaximumVelocityChange =
		std::isfinite(settings.anchorMaximumVelocityChange)
		? std::clamp(settings.anchorMaximumVelocityChange, 0.05f, 10.0f) : 2.3f;
	settings_.shutterPeriodSeconds = std::isfinite(settings.shutterPeriodSeconds)
		? std::clamp(settings.shutterPeriodSeconds, 1.0f, 30.0f) : 3.4f;
	settings_.shutterTravelSeconds = std::isfinite(settings.shutterTravelSeconds)
		? std::clamp(settings.shutterTravelSeconds, 0.1f,
			settings_.shutterPeriodSeconds * 0.45f) : 0.45f;
	const float maximumClosedSeconds = (std::max)(
		0.0f,
		settings_.shutterPeriodSeconds - settings_.shutterTravelSeconds * 2.0f);
	settings_.shutterClosedSeconds = std::isfinite(settings.shutterClosedSeconds)
		? std::clamp(settings.shutterClosedSeconds, 0.0f,
			maximumClosedSeconds) : 1.1f;
	settings_.shutterLiftPadding = std::isfinite(settings.shutterLiftPadding)
		? std::clamp(settings.shutterLiftPadding, 0.1f, 10.0f) : 1.25f;
	settings_.transferCooldownSeconds =
		std::isfinite(settings.transferCooldownSeconds)
		? std::clamp(settings.transferCooldownSeconds, 0.1f, 3.0f) : 0.45f;
	settings_.transferExitPadding = std::isfinite(settings.transferExitPadding)
		? std::clamp(settings.transferExitPadding, 0.01f, 1.0f) : 0.08f;
	settings_.repulsionAcceleration = std::isfinite(settings.repulsionAcceleration)
		? std::clamp(settings.repulsionAcceleration, 0.0f, 240.0f) : 72.0f;
	settings_.repulsionMaximumVelocityChange =
		std::isfinite(settings.repulsionMaximumVelocityChange)
		? std::clamp(settings.repulsionMaximumVelocityChange, 0.05f, 10.0f)
		: 1.4f;
	settings_.repulsionMaximumSpeed =
		std::isfinite(settings.repulsionMaximumSpeed)
		? std::clamp(settings.repulsionMaximumSpeed, 1.0f, 80.0f)
		: 24.0f;
}

bool ObstacleCollisionSystem::Resolve(
	physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle playerBody,
	const physics::BodyHandle* ballBodies,
	std::size_t ballCount,
	const MagnetStageBoxPlacement* obstacles,
	std::size_t obstacleCount,
	float fixedDeltaTime) noexcept
{
	eventCount_ = 0;
	if ((!ballBodies && ballCount > 0) || (!obstacles && obstacleCount > 0) ||
		ballCount > MagnetStageData::kMaximumBallCount ||
		obstacleCount > MagnetStageData::kMaximumObstacleCount ||
		!std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f) {
		return false;
	}
	elapsedSeconds_ += fixedDeltaTime;
	if (!std::isfinite(elapsedSeconds_)) {
		return false;
	}
	if (elapsedSeconds_ > 3600.0f) {
		elapsedSeconds_ = std::fmod(elapsedSeconds_, settings_.shutterPeriodSeconds);
	}
	for (float& cooldown : transferCooldowns_) {
		cooldown = (std::max)(0.0f, cooldown - fixedDeltaTime);
	}

	for (std::size_t index = obstacleCount; index < anchors_.size(); ++index) {
		anchors_[index] = {};
	}
	for (std::size_t obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex) {
		const MagnetStageBoxPlacement& obstacle = obstacles[obstacleIndex];
		if (!IsFinite(obstacle.position) || !IsFinite(obstacle.size) ||
			obstacle.size.x <= 0.0f || obstacle.size.y <= 0.0f ||
			obstacle.size.z <= 0.0f) {
			return false;
		}
		switch (obstacle.obstacleKind) {
		case MagnetObstacleKind::Solid:
			if (!ResolveBoxBody(physicsWorld, playerBody, obstacle)) { return false; }
			for (std::size_t bodyIndex = 0; bodyIndex < ballCount; ++bodyIndex) {
				if (!ResolveBoxBody(physicsWorld, ballBodies[bodyIndex], obstacle)) {
					return false;
				}
			}
			break;
		case MagnetObstacleKind::Chainsaw:
			if (!ResolveBoxBody(physicsWorld, playerBody, obstacle)) { return false; }
			for (std::size_t bodyIndex = 0; bodyIndex < ballCount; ++bodyIndex) {
				if (BodyTouchesBox(physicsWorld, ballBodies[bodyIndex], obstacle) &&
					!AddEvent(EventType::CutChain, ballBodies[bodyIndex], obstacle.id)) {
					return false;
				}
				if (!ResolveBoxBody(physicsWorld, ballBodies[bodyIndex], obstacle)) {
					return false;
				}
			}
			break;
		case MagnetObstacleKind::PinballBumper:
			if (!ResolveBumperBody(physicsWorld, playerBody, obstacle)) { return false; }
			for (std::size_t bodyIndex = 0; bodyIndex < ballCount; ++bodyIndex) {
				if (!ResolveBumperBody(physicsWorld, ballBodies[bodyIndex], obstacle)) {
					return false;
				}
			}
			break;
		case MagnetObstacleKind::Furnace:
			if (!ResolveBoxBody(physicsWorld, playerBody, obstacle)) { return false; }
			for (std::size_t bodyIndex = 0; bodyIndex < ballCount; ++bodyIndex) {
				if (BodyTouchesBox(physicsWorld, ballBodies[bodyIndex], obstacle) &&
					!AddEvent(EventType::DissolveBall, ballBodies[bodyIndex], obstacle.id)) {
					return false;
				}
			}
			break;
		case MagnetObstacleKind::MagneticAnchor:
			if (!UpdateAnchor(physicsWorld, obstacleIndex, obstacle,
				ballBodies, ballCount, fixedDeltaTime)) {
				return false;
			}
			break;
		case MagnetObstacleKind::TimedShutter:
		{
			MagnetStageBoxPlacement runtimeObstacle = obstacle;
			runtimeObstacle.position.y +=
				GetShutterVerticalOffset(obstacleIndex, obstacle);
			if (!ResolveBoxBody(physicsWorld, playerBody, runtimeObstacle)) { return false; }
			for (std::size_t bodyIndex = 0; bodyIndex < ballCount; ++bodyIndex) {
				if (!ResolveBoxBody(
					physicsWorld, ballBodies[bodyIndex], runtimeObstacle)) {
					return false;
				}
			}
			break;
		}
		case MagnetObstacleKind::TransferGate:
		{
			const MagnetStageBoxPlacement* destination = nullptr;
			std::size_t matchingEndpointCount = 0;
			for (std::size_t candidateIndex = 0;
				candidateIndex < obstacleCount;
				++candidateIndex) {
				const MagnetStageBoxPlacement& candidate = obstacles[candidateIndex];
				if (candidateIndex == obstacleIndex ||
					candidate.obstacleKind != MagnetObstacleKind::TransferGate ||
					candidate.transferPairId != obstacle.transferPairId) {
					continue;
				}
				destination = &candidate;
				++matchingEndpointCount;
			}
			if (obstacle.transferPairId == 0 || matchingEndpointCount > 1) {
				return false;
			}
			if (destination &&
				!EmitTransferEvents(
					physicsWorld,
					playerBody,
					ballBodies,
					ballCount,
					obstacle,
					*destination)) {
				return false;
			}
			break;
		}
		case MagnetObstacleKind::RepulsionField:
			for (std::size_t bodyIndex = 0; bodyIndex < ballCount; ++bodyIndex) {
				if (!ApplyRepulsionField(
					physicsWorld,
					ballBodies[bodyIndex],
					obstacle,
					fixedDeltaTime)) {
					return false;
				}
			}
			break;
		default:
			return false;
		}
	}
	return true;
}

bool ObstacleCollisionSystem::IsShutterClosed(std::size_t obstacleIndex) const noexcept
{
	return GetShutterOpenRatio(obstacleIndex) <= kEpsilon;
}

float ObstacleCollisionSystem::GetShutterOpenRatio(
	std::size_t obstacleIndex) const noexcept
{
	if (obstacleIndex >= MagnetStageData::kMaximumObstacleCount ||
		settings_.shutterPeriodSeconds <= 0.0f ||
		settings_.shutterTravelSeconds <= 0.0f) {
		return 0.0f;
	}
	const float phase = std::fmod(
		(std::max)(elapsedSeconds_, 0.0f), settings_.shutterPeriodSeconds);
	const float openingEnd =
		settings_.shutterClosedSeconds + settings_.shutterTravelSeconds;
	const float closingStart =
		settings_.shutterPeriodSeconds - settings_.shutterTravelSeconds;
	if (phase <= settings_.shutterClosedSeconds) {
		return 0.0f;
	}
	const auto smoothStep = [](float ratio) noexcept {
		const float clamped = std::clamp(ratio, 0.0f, 1.0f);
		return clamped * clamped * (3.0f - 2.0f * clamped);
	};
	if (phase < openingEnd) {
		return smoothStep(
			(phase - settings_.shutterClosedSeconds) /
			settings_.shutterTravelSeconds);
	}
	if (phase <= closingStart) {
		return 1.0f;
	}
	return smoothStep(
		(settings_.shutterPeriodSeconds - phase) /
		settings_.shutterTravelSeconds);
}

float ObstacleCollisionSystem::GetShutterVerticalOffset(
	std::size_t obstacleIndex,
	const MagnetStageBoxPlacement& obstacle) const noexcept
{
	if (!IsFinite(obstacle.size) || obstacle.size.y <= 0.0f) {
		return 0.0f;
	}
	return GetShutterOpenRatio(obstacleIndex) *
		(obstacle.size.y + settings_.shutterLiftPadding);
}

physics::BodyHandle ObstacleCollisionSystem::GetAnchoredBody(
	std::size_t obstacleIndex) const noexcept
{
	return obstacleIndex < anchors_.size() ? anchors_[obstacleIndex].body
		: physics::BodyHandle{};
}

float ObstacleCollisionSystem::GetAnchorAttractionRadius(
	const MagnetStageBoxPlacement& obstacle) const noexcept
{
	if (!IsFinite(obstacle.size) || obstacle.size.x <= 0.0f ||
		obstacle.size.z <= 0.0f) {
		return 0.0f;
	}
	return (std::max)(obstacle.size.x, obstacle.size.z) * 0.5f +
		kAnchorAdditionalReach;
}

float ObstacleCollisionSystem::GetRepulsionFieldRadius(
	const MagnetStageBoxPlacement& obstacle) const noexcept
{
	if (!IsFinite(obstacle.size) || obstacle.size.x <= 0.0f ||
		obstacle.size.z <= 0.0f) {
		return 0.0f;
	}
	return (std::max)(obstacle.size.x, obstacle.size.z) * 0.5f;
}

bool ObstacleCollisionSystem::ApplyRepulsionField(
	physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle bodyHandle,
	const MagnetStageBoxPlacement& obstacle,
	float fixedDeltaTime) const noexcept
{
	const physics::SphereBody* body = physicsWorld.GetBody(bodyHandle);
	if (!body || !body->active || body->motionType == physics::MotionType::Static ||
		!HasVerticalOverlap(*body, obstacle)) {
		return true;
	}
	const float radius = GetRepulsionFieldRadius(obstacle);
	if (!std::isfinite(radius) || radius <= kEpsilon) {
		return false;
	}

	Vector3 outward = body->position - obstacle.position;
	outward.y = 0.0f;
	const float distanceSquared = LengthSquaredXZ(outward);
	if (!std::isfinite(distanceSquared)) {
		return false;
	}
	if (distanceSquared > radius * radius) {
		return true;
	}

	float distance = 0.0f;
	if (distanceSquared > kEpsilon) {
		distance = std::sqrt(distanceSquared);
		outward = outward * (1.0f / distance);
	} else {
		outward = body->linearVelocity;
		outward.y = 0.0f;
		const float velocityLengthSquared = LengthSquaredXZ(outward);
		if (velocityLengthSquared > kEpsilon) {
			outward = outward * (1.0f / std::sqrt(velocityLengthSquared));
		} else {
			outward = { 1.0f, 0.0f, 0.0f };
		}
	}

	const float normalizedDistance = std::clamp(distance / radius, 0.0f, 1.0f);
	const float smoothDistance = normalizedDistance * normalizedDistance *
		(3.0f - 2.0f * normalizedDistance);
	const float accelerationRatio = 1.0f - smoothDistance;
	const float velocityChange = (std::min)(
		settings_.repulsionAcceleration * accelerationRatio * fixedDeltaTime,
		settings_.repulsionMaximumVelocityChange);
	if (!std::isfinite(velocityChange) || velocityChange < 0.0f) {
		return false;
	}

	Vector3 velocity = body->linearVelocity + outward * velocityChange;
	const float horizontalSpeedSquared = LengthSquaredXZ(velocity);
	const float maximumSpeedSquared =
		settings_.repulsionMaximumSpeed * settings_.repulsionMaximumSpeed;
	if (!std::isfinite(horizontalSpeedSquared)) {
		return false;
	}
	if (horizontalSpeedSquared > maximumSpeedSquared &&
		horizontalSpeedSquared > kEpsilon) {
		const float scale = settings_.repulsionMaximumSpeed /
			std::sqrt(horizontalSpeedSquared);
		velocity.x *= scale;
		velocity.z *= scale;
	}
	return IsFinite(velocity) &&
		physicsWorld.SetLinearVelocity(bodyHandle, velocity);
}

bool ObstacleCollisionSystem::TeleportBody(
	physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle bodyHandle,
	const MagnetStageBoxPlacement& source,
	const MagnetStageBoxPlacement& destination) noexcept
{
	const physics::SphereBody* body = physicsWorld.GetBody(bodyHandle);
	if (!body || !body->active || body->motionType == physics::MotionType::Static ||
		source.obstacleKind != MagnetObstacleKind::TransferGate ||
		destination.obstacleKind != MagnetObstacleKind::TransferGate ||
		source.transferPairId == 0 ||
		source.transferPairId != destination.transferPairId ||
		!IsFinite(source.position) || !IsFinite(source.size) ||
		!IsFinite(destination.position) || !IsFinite(destination.size) ||
		source.size.x <= 0.0f || source.size.z <= 0.0f ||
		destination.size.x <= 0.0f || destination.size.z <= 0.0f) {
		return false;
	}

	const Vector3 sourceNormal = GetTransferNormal(source);
	const Vector3 sourceTangent = GetTransferTangent(sourceNormal);
	const Vector3 destinationNormal = GetTransferNormal(destination);
	const Vector3 destinationTangent = GetTransferTangent(destinationNormal);
	const float normalSpeed = DotXZ(body->linearVelocity, sourceNormal);
	const float tangentSpeed = DotXZ(body->linearVelocity, sourceTangent);
	Vector3 rotatedVelocity =
		destinationNormal * normalSpeed + destinationTangent * tangentSpeed;
	rotatedVelocity.y = body->linearVelocity.y;
	if (!IsFinite(rotatedVelocity)) {
		return false;
	}

	const Vector3 sourceOffset = body->position - source.position;
	const float positionSide = DotXZ(sourceOffset, sourceNormal);
	const float exitSide = std::abs(normalSpeed) > kEpsilon
		? std::copysign(1.0f, normalSpeed)
		: (std::abs(positionSide) > kEpsilon
			? std::copysign(1.0f, positionSide)
			: 1.0f);
	const float destinationHalfNormal =
		GetHalfExtentAlong(destination, destinationNormal);
	const float destinationHalfTangent =
		GetHalfExtentAlong(destination, destinationTangent);
	const float maximumTangentOffset =
		(std::max)(0.0f, destinationHalfTangent - body->radius);
	const float tangentOffset = std::clamp(
		DotXZ(sourceOffset, sourceTangent),
		-maximumTangentOffset,
		maximumTangentOffset);
	Vector3 exitPosition = destination.position +
		destinationNormal * exitSide *
			(destinationHalfNormal + body->radius + settings_.transferExitPadding) +
		destinationTangent * tangentOffset;
	exitPosition.y = body->position.y;
	if (!IsFinite(exitPosition) ||
		!physicsWorld.SetPosition(bodyHandle, exitPosition) ||
		!physicsWorld.SetLinearVelocity(bodyHandle, rotatedVelocity)) {
		return false;
	}
	return BeginTransferCooldown(bodyHandle);
}

bool ObstacleCollisionSystem::BeginTransferCooldown(
	physics::BodyHandle body) noexcept
{
	if (!body.IsValid() || body.index >= transferCooldowns_.size()) {
		return false;
	}
	transferCooldowns_[body.index] = settings_.transferCooldownSeconds;
	return true;
}

bool ObstacleCollisionSystem::ResolveBoxBody(
	physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle handle,
	const MagnetStageBoxPlacement& obstacle) const noexcept
{
	const physics::SphereBody* body = physicsWorld.GetBody(handle);
	if (!body || !body->active || !HasVerticalOverlap(*body, obstacle)) {
		return true;
	}
	const float minimumX = obstacle.position.x - obstacle.size.x * 0.5f - body->radius;
	const float maximumX = obstacle.position.x + obstacle.size.x * 0.5f + body->radius;
	const float minimumZ = obstacle.position.z - obstacle.size.z * 0.5f - body->radius;
	const float maximumZ = obstacle.position.z + obstacle.size.z * 0.5f + body->radius;
	Vector3 position = body->position;
	Vector3 normal{};
	bool collided = false;
	if (position.x >= minimumX && position.x <= maximumX &&
		position.z >= minimumZ && position.z <= maximumZ) {
		const float distances[] = {
			position.x - minimumX, maximumX - position.x,
			position.z - minimumZ, maximumZ - position.z,
		};
		std::size_t side = 0;
		for (std::size_t index = 1; index < 4; ++index) {
			if (distances[index] < distances[side]) { side = index; }
		}
		switch (side) {
		case 0: position.x = minimumX - kContactOffset; normal = { -1.0f, 0.0f, 0.0f }; break;
		case 1: position.x = maximumX + kContactOffset; normal = { 1.0f, 0.0f, 0.0f }; break;
		case 2: position.z = minimumZ - kContactOffset; normal = { 0.0f, 0.0f, -1.0f }; break;
		default: position.z = maximumZ + kContactOffset; normal = { 0.0f, 0.0f, 1.0f }; break;
		}
		collided = true;
	} else {
		float hitTime = 0.0f;
		if (SweepPointAgainstBoxXZ(body->previousPosition, body->position,
			minimumX, maximumX, minimumZ, maximumZ, hitTime, normal)) {
			position = body->previousPosition +
				(body->position - body->previousPosition) * hitTime + normal * kContactOffset;
			collided = true;
		}
	}
	if (!collided) { return true; }
	Vector3 velocity = body->linearVelocity;
	const float normalSpeed = DotXZ(velocity, normal);
	if (normalSpeed < 0.0f) {
		const Vector3 normalVelocity = normal * normalSpeed;
		velocity = (velocity - normalVelocity) * settings_.solidTangentialDamping -
			normalVelocity * settings_.solidRestitution;
	}
	return physicsWorld.SetPosition(handle, position) &&
		physicsWorld.SetLinearVelocity(handle, velocity);
}

bool ObstacleCollisionSystem::ResolveBumperBody(
	physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle handle,
	const MagnetStageBoxPlacement& obstacle) const noexcept
{
	const physics::SphereBody* body = physicsWorld.GetBody(handle);
	if (!body || !body->active || !HasVerticalOverlap(*body, obstacle)) {
		return true;
	}
	const float bumperRadius = (std::max)(obstacle.size.x, obstacle.size.z) * 0.5f;
	const float contactRadius = bumperRadius + body->radius;
	const Vector3 center{ obstacle.position.x, body->position.y, obstacle.position.z };
	const Vector3 currentOffset = body->position - center;
	Vector3 normal{};
	Vector3 position = body->position;
	bool collided = false;
	const float currentDistanceSquared = LengthSquaredXZ(currentOffset);
	if (currentDistanceSquared <= contactRadius * contactRadius) {
		if (currentDistanceSquared > kEpsilon) {
			normal = currentOffset * (1.0f / std::sqrt(currentDistanceSquared));
		} else {
			const Vector3 fallback = body->linearVelocity * -1.0f;
			const float fallbackLengthSquared = LengthSquaredXZ(fallback);
			normal = fallbackLengthSquared > kEpsilon
				? fallback * (1.0f / std::sqrt(fallbackLengthSquared))
				: Vector3{ 1.0f, 0.0f, 0.0f };
		}
		position = center + normal * (contactRadius + kContactOffset);
		collided = true;
	} else {
		const Vector3 startOffset = body->previousPosition - center;
		const Vector3 delta = body->position - body->previousPosition;
		const float a = LengthSquaredXZ(delta);
		const float b = 2.0f * DotXZ(startOffset, delta);
		const float c = LengthSquaredXZ(startOffset) - contactRadius * contactRadius;
		const float discriminant = b * b - 4.0f * a * c;
		if (a > kEpsilon && discriminant >= 0.0f) {
			const float hitTime = (-b - std::sqrt(discriminant)) / (2.0f * a);
			if (hitTime >= 0.0f && hitTime <= 1.0f) {
				const Vector3 hitPosition = body->previousPosition + delta * hitTime;
				const Vector3 hitOffset = hitPosition - center;
				const float hitLengthSquared = LengthSquaredXZ(hitOffset);
				if (hitLengthSquared > kEpsilon) {
					normal = hitOffset * (1.0f / std::sqrt(hitLengthSquared));
					position = center + normal * (contactRadius + kContactOffset);
					collided = true;
				}
			}
		}
	}
	if (!collided) { return true; }
	Vector3 velocity = body->linearVelocity;
	const float normalSpeed = DotXZ(velocity, normal);
	if (normalSpeed <= 0.0f) {
		const Vector3 tangentialVelocity = velocity - normal * normalSpeed;
		const float exitSpeed = (std::max)(
			-normalSpeed * settings_.bumperRestitution,
			settings_.bumperMinimumExitSpeed);
		velocity = tangentialVelocity + normal * exitSpeed;
		velocity = ClampMagnitudeXZ(velocity, settings_.bumperMaximumSpeed);
	}
	return physicsWorld.SetPosition(handle, position) &&
		physicsWorld.SetLinearVelocity(handle, velocity);
}

bool ObstacleCollisionSystem::BodyTouchesBox(
	const physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle handle,
	const MagnetStageBoxPlacement& obstacle) const noexcept
{
	const physics::SphereBody* body = physicsWorld.GetBody(handle);
	if (!body || !body->active || !HasVerticalOverlap(*body, obstacle)) {
		return false;
	}
	const float minimumX = obstacle.position.x - obstacle.size.x * 0.5f - body->radius;
	const float maximumX = obstacle.position.x + obstacle.size.x * 0.5f + body->radius;
	const float minimumZ = obstacle.position.z - obstacle.size.z * 0.5f - body->radius;
	const float maximumZ = obstacle.position.z + obstacle.size.z * 0.5f + body->radius;
	if (body->position.x >= minimumX && body->position.x <= maximumX &&
		body->position.z >= minimumZ && body->position.z <= maximumZ) {
		return true;
	}
	float hitTime = 0.0f;
	Vector3 hitNormal{};
	return SweepPointAgainstBoxXZ(body->previousPosition, body->position,
		minimumX, maximumX, minimumZ, maximumZ, hitTime, hitNormal);
}

bool ObstacleCollisionSystem::UpdateAnchor(
	physics::PhysicsWorld& physicsWorld,
	std::size_t obstacleIndex,
	const MagnetStageBoxPlacement& obstacle,
	const physics::BodyHandle* ballBodies,
	std::size_t ballCount,
	float fixedDeltaTime) noexcept
{
	if (obstacleIndex >= anchors_.size()) { return false; }
	AnchorState& state = anchors_[obstacleIndex];
	state.cooldownSeconds = (std::max)(0.0f, state.cooldownSeconds - fixedDeltaTime);
	const physics::SphereBody* heldBody = state.body.IsValid()
		? physicsWorld.GetBody(state.body) : nullptr;
	if (!heldBody || !heldBody->active) {
		state.body = {};
		state.elapsedSeconds = 0.0f;
		state.captured = false;
	}
	if (!state.body.IsValid() && state.cooldownSeconds <= 0.0f) {
		const float attractionRadius = GetAnchorAttractionRadius(obstacle);
		float closestDistanceSquared = attractionRadius * attractionRadius;
		for (std::size_t index = 0; index < ballCount; ++index) {
			const physics::SphereBody* candidate = physicsWorld.GetBody(ballBodies[index]);
			if (!candidate || !candidate->active) { continue; }
			const Vector3 offset = obstacle.position - candidate->position;
			const float distanceSquared = LengthSquaredXZ(offset);
			if (std::isfinite(distanceSquared) && distanceSquared <= closestDistanceSquared) {
				closestDistanceSquared = distanceSquared;
				state.body = ballBodies[index];
				state.elapsedSeconds = 0.0f;
				state.captured = false;
			}
		}
	}
	if (!state.body.IsValid()) { return true; }
	heldBody = physicsWorld.GetBody(state.body);
	if (!heldBody || !heldBody->active) { return true; }
	Vector3 offset = obstacle.position - heldBody->position;
	offset.y = 0.0f;
	const float distanceSquared = LengthSquaredXZ(offset);
	state.elapsedSeconds += fixedDeltaTime;
	if (!state.captured &&
		distanceSquared <= kAnchorCaptureDistance * kAnchorCaptureDistance) {
		state.captured = true;
		state.elapsedSeconds = 0.0f;
	}
	if (state.captured) {
		if (state.elapsedSeconds >= settings_.anchorHoldSeconds) {
			state.body = {};
			state.elapsedSeconds = 0.0f;
			state.cooldownSeconds = settings_.anchorCooldownSeconds;
			state.captured = false;
			return true;
		}
		Vector3 capturedPosition = obstacle.position;
		capturedPosition.y = heldBody->position.y;
		return physicsWorld.SetPosition(state.body, capturedPosition) &&
			physicsWorld.SetLinearVelocity(state.body, {});
	}
	if (state.elapsedSeconds >= settings_.anchorAttractTimeoutSeconds) {
		state.body = {};
		state.elapsedSeconds = 0.0f;
		state.cooldownSeconds = settings_.anchorCooldownSeconds;
		return true;
	}
	Vector3 velocityChange =
		(offset * settings_.anchorAcceleration -
		 heldBody->linearVelocity * settings_.anchorDamping) * fixedDeltaTime;
	velocityChange = ClampMagnitudeXZ(
		velocityChange, settings_.anchorMaximumVelocityChange);
	return physicsWorld.SetLinearVelocity(
		state.body, heldBody->linearVelocity + velocityChange);
}

bool ObstacleCollisionSystem::EmitTransferEvents(
	const physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle playerBody,
	const physics::BodyHandle* ballBodies,
	std::size_t ballCount,
	const MagnetStageBoxPlacement& source,
	const MagnetStageBoxPlacement& destination) noexcept
{
	const auto emitForBody = [&](physics::BodyHandle body) noexcept {
		if (!body.IsValid() || body.index >= transferCooldowns_.size() ||
			transferCooldowns_[body.index] > 0.0f ||
			!BodyTouchesBox(physicsWorld, body, source)) {
			return true;
		}
		return AddEvent(
			EventType::EnterTransferGate,
			body,
			source.id,
			destination.id);
	};
	if (!emitForBody(playerBody)) {
		return false;
	}
	for (std::size_t index = 0; index < ballCount; ++index) {
		if (!emitForBody(ballBodies[index])) {
			return false;
		}
	}
	return true;
}

bool ObstacleCollisionSystem::AddEvent(
	EventType type,
	physics::BodyHandle body,
	uint32_t obstacleId,
	uint32_t destinationObstacleId) noexcept
{
	for (std::size_t index = 0; index < eventCount_; ++index) {
		if (events_[index].type == type && IsSameBody(events_[index].body, body)) {
			return true;
		}
	}
	if (eventCount_ >= events_.size()) { return false; }
	events_[eventCount_++] = {
		type, body, obstacleId, destinationObstacleId };
	return true;
}

} // namespace magnet
