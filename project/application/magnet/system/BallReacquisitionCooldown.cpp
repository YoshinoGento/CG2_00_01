#include "application/magnet/system/BallReacquisitionCooldown.h"

#include <algorithm>
#include <cmath>

namespace magnet {

void BallReacquisitionCooldown::Reset() noexcept
{
	remainingSeconds_.fill(0.0f);
}

void BallReacquisitionCooldown::Begin(std::size_t ballIndex) noexcept
{
	if (ballIndex < remainingSeconds_.size()) {
		remainingSeconds_[ballIndex] = kDefaultCooldownSeconds;
	}
}

bool BallReacquisitionCooldown::Update(float deltaTime) noexcept
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return false;
	}
	for (float& remaining : remainingSeconds_) {
		remaining = (std::max)(0.0f, remaining - deltaTime);
	}
	return true;
}

bool BallReacquisitionCooldown::CanReacquire(std::size_t ballIndex) const noexcept
{
	return ballIndex < remainingSeconds_.size() && remainingSeconds_[ballIndex] <= 0.0f;
}

} // namespace magnet
