#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "physics/PhysicsWorld.h"

#include <cstddef>

namespace magnet {

// Resolves horizontal sphere collisions against authored axis-aligned obstacles.
class ObstacleCollisionSystem final {
public:
	struct Settings {
		float restitution = 0.55f;
		float tangentialDamping = 0.88f;
	};

	void SetSettings(const Settings& settings) noexcept;
	[[nodiscard]] bool Resolve(
		physics::PhysicsWorld& physicsWorld,
		const physics::BodyHandle* bodies,
		std::size_t bodyCount,
		const MagnetStageBoxPlacement* obstacles,
		std::size_t obstacleCount) const noexcept;

private:
	[[nodiscard]] bool ResolveBody(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle handle,
		const MagnetStageBoxPlacement& obstacle) const noexcept;
	Settings settings_{};
};

} // namespace magnet
