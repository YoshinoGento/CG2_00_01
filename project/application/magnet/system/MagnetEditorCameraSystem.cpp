#include "application/magnet/system/MagnetEditorCameraSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

} // namespace

void MagnetEditorCameraSystem::Reset() noexcept
{
	zoomScale_ = kDefaultZoomScale;
}

bool MagnetEditorCameraSystem::ApplyWheelDelta(float wheelDelta) noexcept
{
	if (!std::isfinite(wheelDelta) || !std::isfinite(zoomScale_)) {
		return false;
	}

	const float boundedWheelDelta = std::clamp(
		wheelDelta,
		-kMaximumWheelStepsPerFrame,
		kMaximumWheelStepsPerFrame);
	const float nextScale = zoomScale_ *
		std::exp(-boundedWheelDelta * kWheelSensitivity);
	if (!std::isfinite(nextScale)) {
		return false;
	}

	zoomScale_ = std::clamp(
		nextScale,
		kMinimumZoomScale,
		kMaximumZoomScale);
	return true;
}

bool MagnetEditorCameraSystem::TryCalculatePosition(
	const Vector3& focusPosition,
	Vector3& outputPosition) const noexcept
{
	if (!IsFinite(focusPosition) || !std::isfinite(zoomScale_) ||
		zoomScale_ < kMinimumZoomScale || zoomScale_ > kMaximumZoomScale) {
		return false;
	}

	const Vector3 position = {
		focusPosition.x,
		focusPosition.y + kBaseHeight * zoomScale_,
		focusPosition.z - kBaseDepth * zoomScale_,
	};
	if (!IsFinite(position)) {
		return false;
	}

	outputPosition = position;
	return true;
}

} // namespace magnet
