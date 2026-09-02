#include "application/magnet/system/MagneticImpactAttachmentSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {

void MagneticImpactAttachmentSystem::SetSettings(const Settings& settings) noexcept
{
	settings_.enabled = settings.enabled;
	settings_.minimumImpactSpeed = std::isfinite(settings.minimumImpactSpeed)
		? std::clamp(settings.minimumImpactSpeed, 0.0f, 80.0f) : 2.0f;
	settings_.captureMargin = std::isfinite(settings.captureMargin)
		? std::clamp(settings.captureMargin, 0.0f, 0.5f) : 0.08f;
	settings_.releaseGraceSeconds = std::isfinite(settings.releaseGraceSeconds)
		? std::clamp(settings.releaseGraceSeconds, 0.0f, 1.0f) : 0.12f;
}

void MagneticImpactAttachmentSystem::Reset() noexcept
{
	attachedPairs_.fill(false);
	timeSinceRelease_ = 0.0f;
}

void MagneticImpactAttachmentSystem::BeginRelease() noexcept
{
	timeSinceRelease_ = 0.0f;
}

bool MagneticImpactAttachmentSystem::Update(
	physics::PhysicsWorld& physicsWorld,
	const MagnetHandles& magnets,
	float deltaTime) noexcept
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return false;
	}
	timeSinceRelease_ += deltaTime;
	if (!settings_.enabled || timeSinceRelease_ < settings_.releaseGraceSeconds) {
		return true;
	}

	for (std::size_t first = 0; first < magnets.size(); ++first) {
		for (std::size_t second = first + 1; second < magnets.size(); ++second) {
			const std::size_t pairIndex = GetPairIndex(first, second);
			if (attachedPairs_[pairIndex]) {
				continue;
			}
			const physics::SphereBody* bodyA = physicsWorld.GetBody(magnets[first]);
			const physics::SphereBody* bodyB = physicsWorld.GetBody(magnets[second]);
			if (!bodyA || !bodyB || !bodyA->active || !bodyB->active) {
				continue;
			}
			const Vector3 offset = bodyB->position - bodyA->position;
			const float distanceSquared =
				offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
			const float captureDistance =
				bodyA->radius + bodyB->radius + settings_.captureMargin;
			if (!std::isfinite(distanceSquared) ||
				distanceSquared > captureDistance * captureDistance) {
				continue;
			}
			const Vector3 relativeVelocity = bodyB->linearVelocity - bodyA->linearVelocity;
			const float relativeSpeedSquared =
				relativeVelocity.x * relativeVelocity.x +
				relativeVelocity.y * relativeVelocity.y +
				relativeVelocity.z * relativeVelocity.z;
			if (!std::isfinite(relativeSpeedSquared) ||
				relativeSpeedSquared <
					settings_.minimumImpactSpeed * settings_.minimumImpactSpeed) {
				continue;
			}

			physics::DistanceConstraintDesc joint{};
			joint.bodyA = magnets[first];
			joint.bodyB = magnets[second];
			joint.restLength = bodyA->radius + bodyB->radius;
			joint.compliance = 0.00001f;
			joint.maximumCorrection = 0.45f;
			joint.debugDraw = true;
			if (!physicsWorld.CreateDistanceConstraint(joint)) {
				return false;
			}
			attachedPairs_[pairIndex] = true;
		}
	}
	return true;
}

std::size_t MagneticImpactAttachmentSystem::GetAttachmentCount() const noexcept
{
	return static_cast<std::size_t>(std::count(
		attachedPairs_.begin(), attachedPairs_.end(), true));
}

std::size_t MagneticImpactAttachmentSystem::GetPairIndex(
	std::size_t first,
	std::size_t second) noexcept
{
	return first * (2 * kMagnetCount - first - 1) / 2 + (second - first - 1);
}

} // namespace magnet
