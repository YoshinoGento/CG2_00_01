#include "application/magnet/system/ObstacleCollisionSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

constexpr float kContactOffset = 0.002f;
constexpr float kEpsilon = 1.0e-6f;

bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float DotXZ(const Vector3& left, const Vector3& right) noexcept
{
	return left.x * right.x + left.z * right.z;
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

} // namespace

void ObstacleCollisionSystem::SetSettings(const Settings& settings) noexcept
{
	settings_.restitution = std::isfinite(settings.restitution)
		? std::clamp(settings.restitution, 0.0f, 1.0f) : 0.55f;
	settings_.tangentialDamping = std::isfinite(settings.tangentialDamping)
		? std::clamp(settings.tangentialDamping, 0.0f, 1.0f) : 0.88f;
}

bool ObstacleCollisionSystem::Resolve(physics::PhysicsWorld& physicsWorld,
	const physics::BodyHandle* bodies, std::size_t bodyCount,
	const MagnetStageBoxPlacement* obstacles, std::size_t obstacleCount) const noexcept
{
	if ((!bodies && bodyCount > 0) || (!obstacles && obstacleCount > 0)) {
		return false;
	}
	for (std::size_t bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex) {
		for (std::size_t obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex) {
			if (!ResolveBody(physicsWorld, bodies[bodyIndex], obstacles[obstacleIndex])) {
				return false;
			}
		}
	}
	return true;
}

bool ObstacleCollisionSystem::ResolveBody(physics::PhysicsWorld& physicsWorld,
	physics::BodyHandle handle, const MagnetStageBoxPlacement& obstacle) const noexcept
{
	const physics::SphereBody* body = physicsWorld.GetBody(handle);
	if (!body || !body->active) { return true; }
	if (!IsFinite(obstacle.position) || !IsFinite(obstacle.size) ||
		obstacle.size.x <= 0.0f || obstacle.size.y <= 0.0f || obstacle.size.z <= 0.0f) {
		return false;
	}
	const float bottom = obstacle.position.y - obstacle.size.y * 0.5f;
	const float top = obstacle.position.y + obstacle.size.y * 0.5f;
	if (body->position.y + body->radius < bottom || body->position.y - body->radius > top) {
		return true;
	}
	const float minX = obstacle.position.x - obstacle.size.x * 0.5f - body->radius;
	const float maxX = obstacle.position.x + obstacle.size.x * 0.5f + body->radius;
	const float minZ = obstacle.position.z - obstacle.size.z * 0.5f - body->radius;
	const float maxZ = obstacle.position.z + obstacle.size.z * 0.5f + body->radius;
	Vector3 position = body->position;
	Vector3 normal{};
	bool collided = false;
	if (position.x >= minX && position.x <= maxX && position.z >= minZ && position.z <= maxZ) {
		const float distances[] = { position.x - minX, maxX - position.x,
			position.z - minZ, maxZ - position.z };
		std::size_t side = 0;
		for (std::size_t index = 1; index < 4; ++index) {
			if (distances[index] < distances[side]) { side = index; }
		}
		switch (side) {
		case 0: position.x = minX - kContactOffset; normal = { -1.0f, 0.0f, 0.0f }; break;
		case 1: position.x = maxX + kContactOffset; normal = { 1.0f, 0.0f, 0.0f }; break;
		case 2: position.z = minZ - kContactOffset; normal = { 0.0f, 0.0f, -1.0f }; break;
		default: position.z = maxZ + kContactOffset; normal = { 0.0f, 0.0f, 1.0f }; break;
		}
		collided = true;
	} else {
		float hitTime = 0.0f;
		if (SweepPointAgainstBoxXZ(body->previousPosition, body->position,
			minX, maxX, minZ, maxZ, hitTime, normal)) {
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
		velocity = (velocity - normalVelocity) * settings_.tangentialDamping -
			normalVelocity * settings_.restitution;
	}
	return physicsWorld.SetPosition(handle, position) &&
		physicsWorld.SetLinearVelocity(handle, velocity);
}

} // namespace magnet
