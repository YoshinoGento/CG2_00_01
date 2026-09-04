#include "physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace physics {
namespace {

constexpr float kMinimumRadius = 0.01f;
constexpr float kMinimumMass = 0.001f;
constexpr float kMinimumDistanceSquared = 1.0e-10f;
constexpr float kGravityAcceleration = -9.80665f;
constexpr float kGroundContactTolerance = 1.0e-4f;
constexpr float kMinimumBounceSpeed = 0.85f;
constexpr float kMaximumGravityScale = 8.0f;
constexpr float kMaximumSubstepSeconds = 1.0f / 30.0f;
constexpr uint32_t kMaximumSubsteps = 8;
constexpr uint32_t kMaximumSolverIterations = 32;

bool IsFinite(float value) noexcept
{
	return std::isfinite(value);
}

bool IsFinite(const Vector3& value) noexcept
{
	return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

bool IsFinite(const Quaternion& value) noexcept
{
	return IsFinite(value.x) && IsFinite(value.y) &&
		IsFinite(value.z) && IsFinite(value.w);
}

float LengthSquared(const Vector3& value) noexcept
{
	return value.x * value.x + value.y * value.y + value.z * value.z;
}

float LengthSquared(const Quaternion& value) noexcept
{
	return value.x * value.x + value.y * value.y +
		value.z * value.z + value.w * value.w;
}

Quaternion Normalize(const Quaternion& value) noexcept
{
	const float lengthSquared = LengthSquared(value);
	if (!IsFinite(lengthSquared) || lengthSquared <= kMinimumDistanceSquared) {
		return { 0.0f, 0.0f, 0.0f, 1.0f };
	}
	const float inverseLength = 1.0f / std::sqrt(lengthSquared);
	return {
		value.x * inverseLength,
		value.y * inverseLength,
		value.z * inverseLength,
		value.w * inverseLength,
	};
}

Quaternion Multiply(const Quaternion& left, const Quaternion& right) noexcept
{
	return {
		left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
		left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
		left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
		left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
	};
}

float GetSolverInverseMass(const SphereBody& body) noexcept
{
	return body.motionType == MotionType::Dynamic ? body.inverseMass : 0.0f;
}

} // namespace

PhysicsWorld::PhysicsWorld()
{
	bodies_.reserve(kMaximumBodies);
	constraints_.reserve(kMaximumConstraints);
}

void PhysicsWorld::Clear() noexcept
{
	bodies_.clear();
	constraints_.clear();
	++generation_;
	if (generation_ == 0) {
		generation_ = 1;
	}
}

BodyHandle PhysicsWorld::CreateSphereBody(const SphereBodyDesc& desc)
{
	if (bodies_.size() >= kMaximumBodies ||
		!IsFinite(desc.position) || !IsFinite(desc.linearVelocity) ||
		!IsFinite(desc.radius) || desc.radius < kMinimumRadius ||
		!IsFinite(desc.linearDamping) || desc.linearDamping < 0.0f ||
		!IsFinite(desc.planeHeight) ||
		!IsFinite(desc.gravityScale) || desc.gravityScale < 0.0f ||
		desc.gravityScale > kMaximumGravityScale ||
		!IsFinite(desc.groundHeight) ||
		!IsFinite(desc.restitution) || desc.restitution < 0.0f ||
		desc.restitution > 1.0f ||
		!IsFinite(desc.groundFriction) || desc.groundFriction < 0.0f) {
		return {};
	}

	float inverseMass = 0.0f;
	if (desc.motionType == MotionType::Dynamic) {
		if (!IsFinite(desc.mass) || desc.mass < kMinimumMass) {
			return {};
		}
		inverseMass = 1.0f / desc.mass;
	}

	SphereBody body{};
	body.position = desc.position;
	body.previousPosition = desc.position;
	body.linearVelocity = desc.linearVelocity;
	body.radius = desc.radius;
	body.inverseMass = inverseMass;
	body.linearDamping = desc.linearDamping;
	body.planeHeight = desc.lockToHorizontalPlane ? desc.planeHeight : desc.position.y;
	body.gravityScale = desc.gravityScale;
	body.groundHeight = desc.groundHeight;
	body.restitution = desc.restitution;
	body.groundFriction = desc.groundFriction;
	body.motionType = desc.motionType;
	body.lockToHorizontalPlane = desc.lockToHorizontalPlane;
	body.collideWithGround = desc.collideWithGround;
	body.active = desc.active;
	body.generation = generation_;
	if (body.lockToHorizontalPlane) {
		body.position.y = body.planeHeight;
		body.previousPosition.y = body.planeHeight;
		body.linearVelocity.y = 0.0f;
	}

	const BodyHandle handle{ static_cast<uint32_t>(bodies_.size()), generation_ };
	bodies_.push_back(body);
	return handle;
}

bool PhysicsWorld::CreateDistanceConstraint(const DistanceConstraintDesc& desc)
{
	if (constraints_.size() >= kMaximumConstraints ||
		!IsValidHandle(desc.bodyA) || !IsValidHandle(desc.bodyB) ||
		desc.bodyA.index == desc.bodyB.index ||
		!IsFinite(desc.restLength) || desc.restLength < kMinimumRadius ||
		!IsFinite(desc.compliance) || desc.compliance < 0.0f ||
		!IsFinite(desc.maximumCorrection) || desc.maximumCorrection <= 0.0f) {
		return false;
	}

	constraints_.push_back({
		desc.bodyA,
		desc.bodyB,
		desc.restLength,
		desc.compliance,
		desc.maximumCorrection,
		0.0f,
		desc.active,
		desc.debugDraw,
	});
	return true;
}

bool PhysicsWorld::ConfigureDistanceConstraint(
	std::size_t index,
	const DistanceConstraintDesc& desc) noexcept
{
	if (index >= constraints_.size() ||
		!IsValidHandle(desc.bodyA) || !IsValidHandle(desc.bodyB) ||
		desc.bodyA.index == desc.bodyB.index ||
		!IsFinite(desc.restLength) || desc.restLength < kMinimumRadius ||
		!IsFinite(desc.compliance) || desc.compliance < 0.0f ||
		!IsFinite(desc.maximumCorrection) || desc.maximumCorrection <= 0.0f) {
		return false;
	}

	DistanceConstraint& constraint = constraints_[index];
	constraint.bodyA = desc.bodyA;
	constraint.bodyB = desc.bodyB;
	constraint.restLength = desc.restLength;
	constraint.compliance = desc.compliance;
	constraint.maximumCorrection = desc.maximumCorrection;
	constraint.accumulatedLambda = 0.0f;
	constraint.active = desc.active;
	constraint.debugDraw = desc.debugDraw;
	return true;
}

bool PhysicsWorld::SetDistanceConstraintActive(std::size_t index, bool active) noexcept
{
	if (index >= constraints_.size()) {
		return false;
	}
	DistanceConstraint& constraint = constraints_[index];
	constraint.active = active;
	constraint.accumulatedLambda = 0.0f;
	return true;
}

bool PhysicsWorld::SetLinearVelocity(BodyHandle handle, const Vector3& velocity) noexcept
{
	SphereBody* body = GetBodyMutable(handle);
	if (!body || !body->active || body->motionType == MotionType::Static || !IsFinite(velocity)) {
		return false;
	}
	body->linearVelocity = velocity;
	if (body->lockToHorizontalPlane) {
		body->linearVelocity.y = 0.0f;
	}
	return true;
}

bool PhysicsWorld::SetPosition(BodyHandle handle, const Vector3& position) noexcept
{
	SphereBody* body = GetBodyMutable(handle);
	if (!body || !IsFinite(position)) {
		return false;
	}
	body->position = position;
	body->previousPosition = position;
	if (body->lockToHorizontalPlane) {
		body->position.y = body->planeHeight;
		body->previousPosition.y = body->planeHeight;
	}
	return true;
}

bool PhysicsWorld::SetHorizontalPlaneLock(
	BodyHandle handle,
	bool locked,
	float planeHeight) noexcept
{
	SphereBody* body = GetBodyMutable(handle);
	if (!body || !IsFinite(planeHeight)) {
		return false;
	}
	body->lockToHorizontalPlane = locked;
	body->planeHeight = planeHeight;
	body->previousPosition = body->position;
	if (locked) {
		body->position.y = planeHeight;
		body->previousPosition.y = planeHeight;
		body->linearVelocity.y = 0.0f;
	}
	return true;
}

bool PhysicsWorld::SetActive(BodyHandle handle, bool active) noexcept
{
	SphereBody* body = GetBodyMutable(handle);
	if (!body) {
		return false;
	}
	body->active = active;
	body->previousPosition = body->position;
	if (!active) {
		body->linearVelocity = {};
	}
	return true;
}

bool PhysicsWorld::Step(float fixedDeltaTime, uint32_t substepCount, uint32_t solverIterations) noexcept
{
	if (!IsFinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f ||
		fixedDeltaTime > kMaximumSubstepSeconds * static_cast<float>(kMaximumSubsteps) ||
		substepCount == 0 || substepCount > kMaximumSubsteps ||
		solverIterations == 0 || solverIterations > kMaximumSolverIterations) {
		return false;
	}

	const float substepDeltaTime = fixedDeltaTime / static_cast<float>(substepCount);
	for (uint32_t substep = 0; substep < substepCount; ++substep) {
		if (!IntegrateBodies(substepDeltaTime) ||
			!SolveDistanceConstraints(substepDeltaTime, solverIterations) ||
			!ReconstructVelocities(substepDeltaTime) ||
			!ResolveGroundContacts(substepDeltaTime) ||
			!UpdateSphereOrientations(substepDeltaTime)) {
			return false;
		}
	}
	return true;
}

const SphereBody* PhysicsWorld::GetBody(BodyHandle handle) const noexcept
{
	return IsValidHandle(handle) ? &bodies_[handle.index] : nullptr;
}

std::size_t PhysicsWorld::GetActiveConstraintCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		constraints_.begin(),
		constraints_.end(),
		[](const DistanceConstraint& constraint) { return constraint.active; }));
}

SphereBody* PhysicsWorld::GetBodyMutable(BodyHandle handle) noexcept
{
	return IsValidHandle(handle) ? &bodies_[handle.index] : nullptr;
}

bool PhysicsWorld::IsValidHandle(BodyHandle handle) const noexcept
{
	return handle.IsValid() && handle.index < bodies_.size() &&
		handle.generation == generation_ && bodies_[handle.index].generation == handle.generation;
}

bool PhysicsWorld::IntegrateBodies(float deltaTime) noexcept
{
	for (SphereBody& body : bodies_) {
		if (!IsFinite(body.position) || !IsFinite(body.linearVelocity)) {
			return false;
		}
		body.previousPosition = body.position;
		if (!body.active) {
			body.linearVelocity = {};
			body.angularVelocity = {};
			continue;
		}
		if (body.motionType == MotionType::Dynamic &&
			!body.lockToHorizontalPlane && body.gravityScale > 0.0f) {
			body.linearVelocity.y +=
				kGravityAcceleration * body.gravityScale * deltaTime;
		}
		if (body.motionType != MotionType::Static) {
			body.position += body.linearVelocity * deltaTime;
		}
		if (body.lockToHorizontalPlane) {
			body.position.y = body.planeHeight;
		}
		if (!IsFinite(body.position)) {
			return false;
		}
	}
	return true;
}

bool PhysicsWorld::SolveDistanceConstraints(float deltaTime, uint32_t solverIterations) noexcept
{
	const float inverseDeltaTimeSquared = 1.0f / (deltaTime * deltaTime);
	for (DistanceConstraint& constraint : constraints_) {
		constraint.accumulatedLambda = 0.0f;
	}

	for (uint32_t iteration = 0; iteration < solverIterations; ++iteration) {
		for (DistanceConstraint& constraint : constraints_) {
			if (!constraint.active) {
				continue;
			}
			SphereBody* bodyA = GetBodyMutable(constraint.bodyA);
			SphereBody* bodyB = GetBodyMutable(constraint.bodyB);
			if (!bodyA || !bodyB) {
				return false;
			}
			if (!bodyA->active || !bodyB->active) {
				continue;
			}

			const Vector3 delta = bodyB->position - bodyA->position;
			const float distanceSquared = LengthSquared(delta);
			if (!IsFinite(distanceSquared)) {
				return false;
			}
			if (distanceSquared <= kMinimumDistanceSquared) {
				continue;
			}

			const float distance = std::sqrt(distanceSquared);
			const Vector3 direction = delta / distance;
			Vector3 gradientA = -direction;
			Vector3 gradientB = direction;
			if (bodyA->lockToHorizontalPlane) {
				gradientA.y = 0.0f;
			}
			if (bodyB->lockToHorizontalPlane) {
				gradientB.y = 0.0f;
			}
			const float inverseMassA = GetSolverInverseMass(*bodyA);
			const float inverseMassB = GetSolverInverseMass(*bodyB);
			const float alpha = constraint.compliance * inverseDeltaTimeSquared;
			const float denominator =
				inverseMassA * LengthSquared(gradientA) +
				inverseMassB * LengthSquared(gradientB) + alpha;
			if (!IsFinite(denominator) || denominator <= std::numeric_limits<float>::epsilon()) {
				continue;
			}

			const float constraintError = distance - constraint.restLength;
			float deltaLambda =
				(-constraintError - alpha * constraint.accumulatedLambda) / denominator;
			deltaLambda = std::clamp(
				deltaLambda, -constraint.maximumCorrection, constraint.maximumCorrection);
			constraint.accumulatedLambda += deltaLambda;

			bodyA->position += gradientA * (inverseMassA * deltaLambda);
			bodyB->position += gradientB * (inverseMassB * deltaLambda);
			if (bodyA->lockToHorizontalPlane) {
				bodyA->position.y = bodyA->planeHeight;
			}
			if (bodyB->lockToHorizontalPlane) {
				bodyB->position.y = bodyB->planeHeight;
			}
			if (!IsFinite(bodyA->position) || !IsFinite(bodyB->position)) {
				return false;
			}
		}
	}
	return true;
}

bool PhysicsWorld::ReconstructVelocities(float deltaTime) noexcept
{
	for (SphereBody& body : bodies_) {
		if (!body.active) {
			body.linearVelocity = {};
			continue;
		}
		if (body.motionType == MotionType::Dynamic) {
			const float dampingFactor = std::exp(-body.linearDamping * deltaTime);
			body.linearVelocity = ((body.position - body.previousPosition) / deltaTime) * dampingFactor;
		} else if (body.motionType == MotionType::Static) {
			body.linearVelocity = {};
		}
		if (body.lockToHorizontalPlane) {
			body.linearVelocity.y = 0.0f;
		}
		if (!IsFinite(body.linearVelocity)) {
			return false;
		}
	}
	return true;
}

bool PhysicsWorld::ResolveGroundContacts(float deltaTime) noexcept
{
	for (SphereBody& body : bodies_) {
		if (!body.active || body.motionType != MotionType::Dynamic ||
			body.lockToHorizontalPlane || !body.collideWithGround) {
			continue;
		}
		if (!IsFinite(body.position) || !IsFinite(body.linearVelocity) ||
			!IsFinite(body.groundHeight) || !IsFinite(body.restitution) ||
			!IsFinite(body.groundFriction)) {
			return false;
		}

		const float minimumCenterHeight = body.groundHeight + body.radius;
		if (body.position.y > minimumCenterHeight + kGroundContactTolerance) {
			continue;
		}
		body.position.y = minimumCenterHeight;
		if (body.linearVelocity.y < -kMinimumBounceSpeed) {
			body.linearVelocity.y = -body.linearVelocity.y * body.restitution;
		} else if (body.linearVelocity.y < 0.0f) {
			body.linearVelocity.y = 0.0f;
		}
		const float frictionFactor = std::exp(-body.groundFriction * deltaTime);
		body.linearVelocity.x *= frictionFactor;
		body.linearVelocity.z *= frictionFactor;
		if (!IsFinite(body.position) || !IsFinite(body.linearVelocity)) {
			return false;
		}
	}
	return true;
}

bool PhysicsWorld::UpdateSphereOrientations(float deltaTime) noexcept
{
	for (SphereBody& body : bodies_) {
		if (!body.active || body.motionType == MotionType::Static) {
			body.angularVelocity = {};
			continue;
		}
		const Vector3 horizontalVelocity{
			body.linearVelocity.x,
			0.0f,
			body.linearVelocity.z,
		};
		body.angularVelocity = {
			horizontalVelocity.z / body.radius,
			0.0f,
			-horizontalVelocity.x / body.radius,
		};
		const float angularSpeedSquared = LengthSquared(body.angularVelocity);
		if (!IsFinite(angularSpeedSquared)) {
			return false;
		}
		if (angularSpeedSquared > kMinimumDistanceSquared) {
			const float angularSpeed = std::sqrt(angularSpeedSquared);
			const float halfAngle = angularSpeed * deltaTime * 0.5f;
			const float sineScale = std::sin(halfAngle) / angularSpeed;
			const Quaternion deltaRotation{
				body.angularVelocity.x * sineScale,
				body.angularVelocity.y * sineScale,
				body.angularVelocity.z * sineScale,
				std::cos(halfAngle),
			};
			body.orientation = Normalize(Multiply(deltaRotation, body.orientation));
		}
		if (!IsFinite(body.angularVelocity) || !IsFinite(body.orientation)) {
			return false;
		}
	}
	return true;
}

} // namespace physics
