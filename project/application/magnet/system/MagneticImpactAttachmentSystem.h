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
	struct ImpactEvent {
		Vector3 position{};
		float relativeSpeed = 0.0f;
	};
	using ImpactEvents = std::array<ImpactEvent, kPairCount>;

	struct Settings {
		bool enabled = true;
		float minimumImpactSpeed = 2.0f;
		float captureMargin = 0.08f;
		float releaseGraceSeconds = 0.12f;
	};

	void SetSettings(const Settings& settings) noexcept;
	void Reset() noexcept;
	void BeginRelease() noexcept;
	[[nodiscard]] bool DetachBody(
		physics::PhysicsWorld& physicsWorld,
		physics::BodyHandle body) noexcept;
	[[nodiscard]] bool Update(
		physics::PhysicsWorld& physicsWorld,
		const MagnetHandles& magnets,
		float deltaTime) noexcept;
	[[nodiscard]] std::size_t GetAttachmentCount() const noexcept;
	[[nodiscard]] const ImpactEvents& GetImpactEvents() const noexcept { return impactEvents_; }
	[[nodiscard]] std::size_t GetImpactEventCount() const noexcept { return impactEventCount_; }

private:
	Settings settings_{};
	std::array<bool, kPairCount> attachedPairs_{};
	std::array<physics::BodyHandle, kPairCount> attachedBodyA_{};
	std::array<physics::BodyHandle, kPairCount> attachedBodyB_{};
	std::array<std::size_t, kPairCount> attachmentConstraintIndices_{};
	ImpactEvents impactEvents_{};
	std::size_t impactEventCount_ = 0;
	float timeSinceRelease_ = 0.0f;
};

} // namespace magnet
