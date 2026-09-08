#pragma once

#include "math/Struct.h"

#include <array>
#include <cstddef>

class LineDrawer;
class ParticleManager;

namespace magnet {
class GoalCelebrationEffectSystem final {
public:
	[[nodiscard]] bool Initialize(ParticleManager* particleManager) noexcept;
	void Finalize() noexcept;
	void Reset() noexcept;
	void Play(const Vector3& position, std::size_t scoredBallCount);
	void Update(float deltaTime) noexcept;
	void Draw(LineDrawer& lineDrawer) const;

private:
	struct Celebration {
		Vector3 position{};
		float elapsed = 0.0f;
		float intensity = 1.0f;
		bool active = false;
	};
	static constexpr std::size_t kMaximumCelebrations = 4;
	ParticleManager* particleManager_ = nullptr;
	std::array<Celebration, kMaximumCelebrations> celebrations_{};
	std::size_t nextSlot_ = 0;
	uint32_t particleSeed_ = 1;
};
} // namespace magnet
