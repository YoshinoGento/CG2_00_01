#include "application/magnet/system/MagneticImpactAttachmentSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
	attachedBodyA_.fill({});
	attachedBodyB_.fill({});
	attachmentConstraintIndices_.fill((std::numeric_limits<std::size_t>::max)());
	impactEvents_.fill({});
	impactEventCount_ = 0;
	timeSinceRelease_ = 0.0f;
}

void MagneticImpactAttachmentSystem::BeginRelease() noexcept
{
	timeSinceRelease_ = 0.0f;
}

bool MagneticImpactAttachmentSystem::DetachBody(
	physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle body) noexcept
{
	for (std::size_t pairIndex = 0; pairIndex < attachedPairs_.size(); ++pairIndex) {
		if (!attachedPairs_[pairIndex]) {
			continue;
		}
		const bool matchesA = attachedBodyA_[pairIndex].index == body.index &&
			attachedBodyA_[pairIndex].generation == body.generation;
		const bool matchesB = attachedBodyB_[pairIndex].index == body.index &&
			attachedBodyB_[pairIndex].generation == body.generation;
		if (!matchesA && !matchesB) {
			continue;
		}
		if (!physicsWorld.SetDistanceConstraintActive(
			attachmentConstraintIndices_[pairIndex], false)) {
			return false;
		}
		attachedPairs_[pairIndex] = false;
		attachedBodyA_[pairIndex] = {};
		attachedBodyB_[pairIndex] = {};
		attachmentConstraintIndices_[pairIndex] =
			(std::numeric_limits<std::size_t>::max)();
	}
	return true;
}

bool MagneticImpactAttachmentSystem::Update(
	physics::PhysicsWorld& physicsWorld,
	const MagnetHandles& magnets,
	float deltaTime) noexcept
{
	impactEventCount_ = 0;
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return false;
	}
	timeSinceRelease_ += deltaTime;
	if (!settings_.enabled || timeSinceRelease_ < settings_.releaseGraceSeconds) {
		return true;
	}

	for (std::size_t first = 0; first < magnets.size(); ++first) {
		for (std::size_t second = first + 1; second < magnets.size(); ++second) {
			std::size_t pairIndex = attachedPairs_.size();
			bool alreadyAttached = false;
			for (std::size_t recordIndex = 0; recordIndex < attachedPairs_.size(); ++recordIndex) {
				if (!attachedPairs_[recordIndex]) {
					if (pairIndex == attachedPairs_.size()) {
						pairIndex = recordIndex;
					}
					continue;
				}
				const bool sameOrder =
					attachedBodyA_[recordIndex].index == magnets[first].index &&
					attachedBodyA_[recordIndex].generation == magnets[first].generation &&
					attachedBodyB_[recordIndex].index == magnets[second].index &&
					attachedBodyB_[recordIndex].generation == magnets[second].generation;
				const bool reverseOrder =
					attachedBodyA_[recordIndex].index == magnets[second].index &&
					attachedBodyA_[recordIndex].generation == magnets[second].generation &&
					attachedBodyB_[recordIndex].index == magnets[first].index &&
					attachedBodyB_[recordIndex].generation == magnets[first].generation;
				if (sameOrder || reverseOrder) {
					alreadyAttached = true;
					break;
				}
			}
			if (alreadyAttached || pairIndex == attachedPairs_.size()) {
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
			const std::size_t constraintIndex = physicsWorld.GetConstraintCount();
			if (!physicsWorld.CreateDistanceConstraint(joint)) {
				return false;
			}
			attachedPairs_[pairIndex] = true;
			attachedBodyA_[pairIndex] = magnets[first];
			attachedBodyB_[pairIndex] = magnets[second];
			attachmentConstraintIndices_[pairIndex] = constraintIndex;
			if (impactEventCount_ < impactEvents_.size()) {
				ImpactEvent& event = impactEvents_[impactEventCount_++];
				event.position = (bodyA->position + bodyB->position) * 0.5f;
				event.relativeSpeed = std::sqrt(relativeSpeedSquared);
			}
		}
	}
	return true;
}

std::size_t MagneticImpactAttachmentSystem::GetAttachmentCount() const noexcept
{
	return static_cast<std::size_t>(std::count(
		attachedPairs_.begin(), attachedPairs_.end(), true));
}

} // namespace magnet
