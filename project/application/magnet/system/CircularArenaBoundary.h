#pragma once

#include "physics/PhysicsWorld.h"

#include <cstddef>

namespace magnet {

// Resolves horizontal sphere impacts against the inside of a circular arena wall.
class CircularArenaBoundary final {
public:
	struct Settings {
		float radius = 10.0f;
		float restitution = 0.72f;
		float tangentialDamping = 0.96f;
	};

	void SetSettings(const Settings& settings) noexcept;
	[[nodiscard]] bool Resolve(
		physics::PhysicsWorld& physicsWorld,
		const physics::BodyHandle* bodies,
		std::size_t bodyCount) const noexcept;
	[[nodiscard]] float GetRadius() const noexcept { return settings_.radius; }

private:
	Settings settings_{};
};

} // namespace magnet
