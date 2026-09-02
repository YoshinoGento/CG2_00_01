#include "application/magnet/system/CircularArenaBoundary.h"

#include <algorithm>
#include <cmath>

namespace magnet {

void CircularArenaBoundary::SetSettings(const Settings& settings) noexcept
{
	settings_.radius = std::isfinite(settings.radius)
		? std::clamp(settings.radius, 2.0f, 40.0f) : 10.0f;
	settings_.restitution = std::isfinite(settings.restitution)
		? std::clamp(settings.restitution, 0.0f, 1.0f) : 0.72f;
	settings_.tangentialDamping = std::isfinite(settings.tangentialDamping)
		? std::clamp(settings.tangentialDamping, 0.0f, 1.0f) : 0.96f;
}

bool CircularArenaBoundary::Resolve(
	physics::PhysicsWorld& physicsWorld,
	const physics::BodyHandle* bodies,
	std::size_t bodyCount) const noexcept
{
	if (!bodies && bodyCount > 0) {
		return false;
	}
	for (std::size_t index = 0; index < bodyCount; ++index) {
		const physics::SphereBody* body = physicsWorld.GetBody(bodies[index]);
		if (!body || !body->active) {
			continue;
		}
		const float allowedRadius = (std::max)(0.01f, settings_.radius - body->radius);
		const float distanceSquared =
			body->position.x * body->position.x + body->position.z * body->position.z;
		if (!std::isfinite(distanceSquared) ||
			distanceSquared <= allowedRadius * allowedRadius) {
			continue;
		}
		const float distance = std::sqrt(distanceSquared);
		if (distance <= 0.0f) {
			continue;
		}
		const Vector3 normal = { body->position.x / distance, 0.0f, body->position.z / distance };
		const Vector3 correctedPosition = {
			normal.x * allowedRadius,
			body->position.y,
			normal.z * allowedRadius,
		};
		Vector3 correctedVelocity = body->linearVelocity;
		const float outwardSpeed =
			correctedVelocity.x * normal.x + correctedVelocity.z * normal.z;
		if (outwardSpeed > 0.0f) {
			const Vector3 normalVelocity = normal * outwardSpeed;
			const Vector3 tangentVelocity = correctedVelocity - normalVelocity;
			correctedVelocity =
				tangentVelocity * settings_.tangentialDamping -
				normalVelocity * settings_.restitution;
		}
		if (!physicsWorld.SetPosition(bodies[index], correctedPosition) ||
			!physicsWorld.SetLinearVelocity(bodies[index], correctedVelocity)) {
			return false;
		}
	}
	return true;
}

} // namespace magnet
