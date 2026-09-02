#pragma once

#include "physics/PhysicsWorld.h"

#include <array>
#include <cstddef>

namespace magnet {

// Creates permanent magnetic joints when released chain balls hit each other.
class MagneticImpactAttachmentSystem final {
public:
	static constexpr std::size_t kMagnetCount = 8;
	static constexpr std::size_t kPairCount =
		(kMagnetCount * (kMagnetCount - 1)) / 2;
	using MagnetHandles = std::array<physics::BodyHandle, kMagnetCount>;

	struct Settings {
		bool enabled = true;
		float minimumImpactSpeed = 2.0f;
		float captureMargin = 0.08f;
		float releaseGraceSeconds = 0.12f;
	};

	void SetSettings(const Settings& settings) noexcept;
	void Reset() noexcept;
	void BeginRelease() noexcept;
	[[nodiscard]] bool Update(
		physics::PhysicsWorld& physicsWorld,
		const MagnetHandles& magnets,
		float deltaTime) noexcept;
	[[nodiscard]] std::size_t GetAttachmentCount() const noexcept;

private:
	[[nodiscard]] static std::size_t GetPairIndex(
		std::size_t first,
		std::size_t second) noexcept;

	Settings settings_{};
	std::array<bool, kPairCount> attachedPairs_{};
	float timeSinceRelease_ = 0.0f;
};

} // namespace magnet
