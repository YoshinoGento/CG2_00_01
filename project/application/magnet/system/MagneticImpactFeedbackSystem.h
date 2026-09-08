#pragma once

#include "application/magnet/system/MagneticImpactAttachmentSystem.h"

#include <array>
#include <cstddef>

class LineDrawer;

namespace magnet {

// Keeps magnetic impact presentation separate from collision and attachment physics.
class MagneticImpactFeedbackSystem final {
public:
	static constexpr std::size_t kMaximumEffects = 8;

	void Reset() noexcept;
	void AddImpacts(
		const MagneticImpactAttachmentSystem::ImpactEvents& events,
		std::size_t eventCount) noexcept;
	void Update(float deltaTime) noexcept;
	void Draw(LineDrawer& lineDrawer) const;
	[[nodiscard]] Vector3 GetCameraShakeOffset() const noexcept;
	[[nodiscard]] std::size_t GetActiveEffectCount() const noexcept;

private:
	struct Effect {
		Vector3 position{};
		float intensity = 0.0f;
		float age = 0.0f;
		bool active = false;
	};

	std::array<Effect, kMaximumEffects> effects_{};
	float elapsedSeconds_ = 0.0f;
	float shakeIntensity_ = 0.0f;
};

} // namespace magnet
