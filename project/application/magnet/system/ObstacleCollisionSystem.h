#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "physics/PhysicsWorld.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace magnet {

// Owns bounded runtime interaction for every authored obstacle. It reports
// topology-changing contacts; MagnetChainSystem remains the chain-state owner.
class ObstacleCollisionSystem final {
public:
	enum class EventType : uint8_t { CutChain, DissolveBall, EnterTransferGate };

	struct Event {
		EventType type = EventType::CutChain;
		physics::BodyHandle body{};
		uint32_t obstacleId = 0;
		uint32_t destinationObstacleId = 0;
	};

	static constexpr std::size_t kMaximumEventCount =
		MagnetStageData::kMaximumBallCount * 3 + 1;
	using Events = std::array<Event, kMaximumEventCount>;
	struct ImpactEvent {
		physics::BodyHandle body{};
		Vector3 position{};
		float relativeSpeed = 0.0f;
	};
	static constexpr std::size_t kMaximumImpactEventCount =
		MagnetStageData::kMaximumBallCount + 1;
	using ImpactEvents = std::array<ImpactEvent, kMaximumImpactEventCount>;

	struct Settings {
		float solidRestitution = 0.55f;
		float solidTangentialDamping = 0.88f;
		float bumperRestitution = 1.45f;
		float bumperMinimumExitSpeed = 8.5f;
		float bumperMaximumSpeed = 30.0f;
		float anchorHoldSeconds = 1.0f;
		float anchorAttractTimeoutSeconds = 1.75f;
		float anchorCooldownSeconds = 0.75f;
		float anchorAcceleration = 90.0f;
		float anchorDamping = 8.5f;
		float anchorMaximumVelocityChange = 2.3f;
		float shutterPeriodSeconds = 3.4f;
		float shutterClosedSeconds = 1.1f;
		float shutterTravelSeconds = 0.45f;
		float shutterLiftPadding = 1.25f;
		float transferCooldownSeconds = 0.45f;
		float transferExitPadding = 0.08f;
		float repulsionAcceleration = 72.0f;
		float repulsionMaximumVelocityChange = 1.4f;
		float repulsionMaximumSpeed = 24.0f;
	};

	void Reset() noexcept;
	void SetSettings(const Settings& settings) noexcept;
	[[nodiscard]] bool Resolve(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle playerBody,
		const physics::BodyHandle* ballBodies,
		std::size_t ballCount,
		const MagnetStageBoxPlacement* obstacles,
		std::size_t obstacleCount,
		float fixedDeltaTime) noexcept;

	[[nodiscard]] const Events& GetEvents() const noexcept { return events_; }
	[[nodiscard]] std::size_t GetEventCount() const noexcept { return eventCount_; }
	[[nodiscard]] const ImpactEvents& GetImpactEvents() const noexcept {
		return impactEvents_;
	}
	[[nodiscard]] std::size_t GetImpactEventCount() const noexcept {
		return impactEventCount_;
	}
	[[nodiscard]] bool IsShutterClosed(std::size_t obstacleIndex) const noexcept;
	[[nodiscard]] float GetShutterOpenRatio(std::size_t obstacleIndex) const noexcept;
	[[nodiscard]] float GetShutterVerticalOffset(
		std::size_t obstacleIndex,
		const MagnetStageBoxPlacement& obstacle) const noexcept;
	[[nodiscard]] physics::BodyHandle GetAnchoredBody(
		std::size_t obstacleIndex) const noexcept;
	[[nodiscard]] float GetAnchorAttractionRadius(
		const MagnetStageBoxPlacement& obstacle) const noexcept;
	[[nodiscard]] float GetRepulsionFieldRadius(
		const MagnetStageBoxPlacement& obstacle) const noexcept;
	[[nodiscard]] bool TeleportBody(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle body,
		const MagnetStageBoxPlacement& source,
		const MagnetStageBoxPlacement& destination) noexcept;
	[[nodiscard]] bool BeginTransferCooldown(
		physics::BodyHandle body) noexcept;

private:
	struct AnchorState {
		physics::BodyHandle body{};
		float elapsedSeconds = 0.0f;
		float cooldownSeconds = 0.0f;
		bool captured = false;
	};

	[[nodiscard]] bool ResolveBoxBody(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle handle,
		const MagnetStageBoxPlacement& obstacle,
		bool reportImpact) noexcept;
	[[nodiscard]] bool ResolveBumperBody(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle handle,
		const MagnetStageBoxPlacement& obstacle) const noexcept;
	[[nodiscard]] bool BodyTouchesBox(
		const physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle handle,
		const MagnetStageBoxPlacement& obstacle) const noexcept;
	[[nodiscard]] bool UpdateAnchor(
		physics::PhysicsWorld& physicsWorld,
		std::size_t obstacleIndex,
		const MagnetStageBoxPlacement& obstacle,
		const physics::BodyHandle* ballBodies,
		std::size_t ballCount,
		float fixedDeltaTime) noexcept;
	[[nodiscard]] bool ApplyRepulsionField(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle body,
		const MagnetStageBoxPlacement& obstacle,
		float fixedDeltaTime) const noexcept;
	[[nodiscard]] bool EmitTransferEvents(
		const physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle playerBody,
		const physics::BodyHandle* ballBodies,
		std::size_t ballCount,
		const MagnetStageBoxPlacement& source,
		const MagnetStageBoxPlacement& destination) noexcept;
	[[nodiscard]] bool AddEvent(
		EventType type,
		physics::BodyHandle body,
		uint32_t obstacleId,
		uint32_t destinationObstacleId = 0) noexcept;
	void AddImpactEvent(
		physics::BodyHandle body,
		const Vector3& position,
		float relativeSpeed) noexcept;

	Settings settings_{};
	Events events_{};
	ImpactEvents impactEvents_{};
	std::array<AnchorState, MagnetStageData::kMaximumObstacleCount> anchors_{};
	std::array<float, physics::PhysicsWorld::kMaximumBodies> transferCooldowns_{};
	std::size_t eventCount_ = 0;
	std::size_t impactEventCount_ = 0;
	float elapsedSeconds_ = 0.0f;
};

} // namespace magnet
