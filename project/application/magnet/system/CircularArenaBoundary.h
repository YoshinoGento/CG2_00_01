#pragma once

#include "physics/PhysicsWorld.h"

#include <array>
#include <cstddef>

namespace magnet {

// Resolves horizontal sphere impacts against the inside of a circular arena wall.
class CircularArenaBoundary final {
public:
	struct ImpactEvent {
		physics::BodyHandle body{};
		Vector3 position{};
		float relativeSpeed = 0.0f;
	};
	static constexpr std::size_t kMaximumImpactEventCount =
		physics::PhysicsWorld::kMaximumBodies;
	using ImpactEvents = std::array<ImpactEvent, kMaximumImpactEventCount>;

	struct Settings {
		float radius = 10.0f;
		float restitution = 0.72f;
		float tangentialDamping = 0.96f;
	};

	void SetSettings(const Settings& settings) noexcept;
	[[nodiscard]] bool Resolve(
		physics::PhysicsWorld& physicsWorld,
		const physics::BodyHandle* bodies,
		std::size_t bodyCount) noexcept;
	[[nodiscard]] float GetRadius() const noexcept { return settings_.radius; }
	[[nodiscard]] const ImpactEvents& GetImpactEvents() const noexcept {
		return impactEvents_;
	}
	[[nodiscard]] std::size_t GetImpactEventCount() const noexcept {
		return impactEventCount_;
	}

private:
	Settings settings_{};
	ImpactEvents impactEvents_{};
	std::size_t impactEventCount_ = 0;
};

} // namespace magnet
