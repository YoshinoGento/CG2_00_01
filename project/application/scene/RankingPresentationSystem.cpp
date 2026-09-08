#include "application/scene/RankingPresentationSystem.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kMaximumDeltaSeconds = 0.1f;
constexpr float kTimeWrapSeconds = 120.0f;
constexpr Vector4 kLeftBallColor{ 0.27f, 0.84f, 1.0f, 1.0f };
constexpr Vector4 kRightBallColor{ 0.35f, 1.0f, 0.72f, 1.0f };
constexpr Vector4 kGoldColor{ 1.0f, 0.82f, 0.24f, 1.0f };
constexpr Vector4 kCyanWhiteColor{ 0.72f, 1.0f, 1.0f, 1.0f };

[[nodiscard]] float SmoothStep(float value) noexcept
{
	const float clamped = std::clamp(value, 0.0f, 1.0f);
	return clamped * clamped * (3.0f - 2.0f * clamped);
}

[[nodiscard]] Vector4 LerpColor(
	const Vector4& from, const Vector4& to, float amount) noexcept
{
	const float t = std::clamp(amount, 0.0f, 1.0f);
	return {
		from.x + (to.x - from.x) * t,
		from.y + (to.y - from.y) * t,
		from.z + (to.z - from.z) * t,
		from.w + (to.w - from.w) * t,
	};
}
} // namespace

void RankingPresentationSystem::Reset() noexcept
{
	elapsedSeconds_ = 0.0f;
	frame_ = {};
	Update(0.0f);
}

void RankingPresentationSystem::Update(float deltaSeconds) noexcept
{
	if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0f) {
		elapsedSeconds_ += std::clamp(deltaSeconds, 0.0f, kMaximumDeltaSeconds);
		if (elapsedSeconds_ >= kTimeWrapSeconds) {
			elapsedSeconds_ = std::fmod(elapsedSeconds_, kTimeWrapSeconds);
		}
	}

	const float time = elapsedSeconds_;
	const float intro = SmoothStep(time / 0.75f);
	const float pulse = 0.5f + 0.5f * std::sin(time * 4.8f);
	frame_.headingScaleFactor =
		1.0f + 0.035f * std::sin(time * 2.2f) +
		0.10f * std::exp(-3.2f * time) * std::sin(time * 12.0f);
	frame_.highlightedRowScale = 1.0f + 0.14f * pulse;
	frame_.markerScale = 0.92f + 0.12f * pulse;
	frame_.highlightedRowColor = LerpColor(kGoldColor, kCyanWhiteColor, pulse);
	frame_.playerPosition = { 0.15f, -1.18f, -0.02f };
	frame_.playerScale = (0.17f + 0.04f * intro) *
		(1.0f + 0.06f * std::sin(time * 3.2f));

	constexpr std::size_t kBallsPerSide =
		RankingPresentationFrame::kDecorationBallCount / 2;
	for (std::size_t index = 0;
		index < RankingPresentationFrame::kDecorationBallCount;
		++index) {
		const bool isRight = index >= kBallsPerSide;
		const std::size_t linkIndex = isRight ? index - kBallsPerSide : index;
		const float link = static_cast<float>(linkIndex + 1);
		const float side = isRight ? 1.0f : -1.0f;
		const float outwardRatio = link / static_cast<float>(kBallsPerSide);
		const float distance = (0.12f + 0.34f * link) * intro;
		const float phase = time * 3.3f - link * 0.55f +
			(isRight ? kPi : 0.0f);
		const float waveAmplitude = (0.045f + 0.16f * outwardRatio) * intro;

		RankingDecorationTransform& ball = frame_.balls[index];
		ball.position = {
			frame_.playerPosition.x + side * distance,
			frame_.playerPosition.y + 0.035f * link * intro +
				std::sin(phase) * waveAmplitude,
			frame_.playerPosition.z + std::cos(phase) * 0.045f * intro,
		};
		ball.scale = (0.115f + 0.012f * outwardRatio) *
			(0.08f + 0.92f * intro) *
			(1.0f + 0.08f * std::sin(phase + 0.7f));
		ball.color = isRight ? kRightBallColor : kLeftBallColor;
	}
}
