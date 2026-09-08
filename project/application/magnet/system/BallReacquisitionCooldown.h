#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"

#include <array>
#include <cstddef>

namespace magnet {

class BallReacquisitionCooldown final {
public:
	static constexpr float kDefaultCooldownSeconds = 1.0f;

	void Reset() noexcept;
	void Begin(std::size_t ballIndex) noexcept;
	[[nodiscard]] bool Update(float deltaTime) noexcept;
	[[nodiscard]] bool CanReacquire(std::size_t ballIndex) const noexcept;

private:
	std::array<float, MagnetStageData::kMaximumBallCount> remainingSeconds_{};
};

} // namespace magnet
