#pragma once

#include "math/Struct.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace physics {

enum class MotionType : uint8_t {
	Static,
	Kinematic,
	Dynamic,
};

struct BodyHandle {
	static constexpr uint32_t kInvalidIndex = UINT32_MAX;

	uint32_t index = kInvalidIndex;
	uint32_t generation = 0;

	[[nodiscard]] bool IsValid() const noexcept { return index != kInvalidIndex; }
};

struct SphereBodyDesc {
	Vector3 position{};
	Vector3 linearVelocity{};
	float radius = 0.5f;
	float mass = 1.0f;
	float linearDamping = 0.25f;
	float planeHeight = 0.5f;
	MotionType motionType = MotionType::Dynamic;
	bool lockToHorizontalPlane = true;
	bool active = true;
};

struct SphereBody {
	Vector3 position{};
	Vector3 previousPosition{};
	Vector3 linearVelocity{};
	float radius = 0.5f;
	float inverseMass = 1.0f;
	float linearDamping = 0.25f;
	float planeHeight = 0.5f;
	MotionType motionType = MotionType::Dynamic;
	bool lockToHorizontalPlane = true;
	bool active = true;
	uint32_t generation = 0;
};

struct DistanceConstraintDesc {
	BodyHandle bodyA{};
	BodyHandle bodyB{};
	float restLength = 1.0f;
	float compliance = 0.0f;
	float maximumCorrection = 0.5f;
};

struct DistanceConstraint {
	BodyHandle bodyA{};
	BodyHandle bodyB{};
	float restLength = 1.0f;
	float compliance = 0.0f;
	float maximumCorrection = 0.5f;
	float accumulatedLambda = 0.0f;
	bool active = true;
};

// CPU-only fixed-step sphere and distance-constraint simulation.
// Gameplay topology and rendering intentionally remain outside this class.
class PhysicsWorld final {
public:
	static constexpr std::size_t kMaximumBodies = 128;
	static constexpr std::size_t kMaximumConstraints = 256;

	PhysicsWorld();

	void Clear() noexcept;
	[[nodiscard]] BodyHandle CreateSphereBody(const SphereBodyDesc& desc);
	[[nodiscard]] bool CreateDistanceConstraint(const DistanceConstraintDesc& desc);
	[[nodiscard]] bool SetDistanceConstraintActive(std::size_t index, bool active) noexcept;
	[[nodiscard]] bool SetLinearVelocity(BodyHandle handle, const Vector3& velocity) noexcept;
	[[nodiscard]] bool SetPosition(BodyHandle handle, const Vector3& position) noexcept;
	[[nodiscard]] bool SetActive(BodyHandle handle, bool active) noexcept;
	[[nodiscard]] bool Step(float fixedDeltaTime, uint32_t substepCount, uint32_t solverIterations) noexcept;

	[[nodiscard]] const SphereBody* GetBody(BodyHandle handle) const noexcept;
	[[nodiscard]] const std::vector<DistanceConstraint>& GetConstraints() const noexcept { return constraints_; }
	[[nodiscard]] std::size_t GetBodyCount() const noexcept { return bodies_.size(); }
	[[nodiscard]] std::size_t GetConstraintCount() const noexcept { return constraints_.size(); }
	[[nodiscard]] std::size_t GetActiveConstraintCount() const noexcept;

private:
	[[nodiscard]] SphereBody* GetBodyMutable(BodyHandle handle) noexcept;
	[[nodiscard]] bool IsValidHandle(BodyHandle handle) const noexcept;
	[[nodiscard]] bool IntegrateBodies(float deltaTime) noexcept;
	[[nodiscard]] bool SolveDistanceConstraints(float deltaTime, uint32_t solverIterations) noexcept;
	[[nodiscard]] bool ReconstructVelocities(float deltaTime) noexcept;

	std::vector<SphereBody> bodies_;
	std::vector<DistanceConstraint> constraints_;
	uint32_t generation_ = 1;
};

} // namespace physics
