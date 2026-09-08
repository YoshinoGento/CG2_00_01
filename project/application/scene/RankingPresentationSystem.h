#pragma once

#include "math/Struct.h"

#include <array>
#include <cstddef>

struct RankingDecorationTransform {
	Vector3 position{};
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float scale = 0.1f;
};

struct RankingPresentationFrame {
	static constexpr std::size_t kDecorationBallCount = 10;

	float headingScaleFactor = 1.0f;
	float highlightedRowScale = 1.0f;
	float markerScale = 1.0f;
	Vector4 highlightedRowColor{ 1.0f, 0.82f, 0.24f, 1.0f };
	Vector3 playerPosition{ 0.15f, -1.18f, 0.0f };
	float playerScale = 0.2f;
	std::array<RankingDecorationTransform, kDecorationBallCount> balls{};
};

// Owns bounded, presentation-only motion for the Ranking result screen.
class RankingPresentationSystem final {
public:
	void Reset() noexcept;
	void Update(float deltaSeconds) noexcept;
	[[nodiscard]] const RankingPresentationFrame& GetFrame() const noexcept {
		return frame_;
	}

private:
	float elapsedSeconds_ = 0.0f;
	RankingPresentationFrame frame_{};
};
