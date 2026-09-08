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
	panOffset_ = {};
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

bool MagnetEditorCameraSystem::ApplyPanDrag(
	const Vector2& normalizedDelta) noexcept
{
	if (!std::isfinite(normalizedDelta.x) || !std::isfinite(normalizedDelta.y) ||
		!IsFinite(panOffset_)) {
		return false;
	}
	// Grab-style panning: the stage follows the dragged mouse direction.
	panOffset_.x -= normalizedDelta.x * kPanDragScale * zoomScale_;
	panOffset_.z += normalizedDelta.y * kPanDragScale * zoomScale_;
	panOffset_.x = std::clamp(
		panOffset_.x, -kMaximumPanDistance, kMaximumPanDistance);
	panOffset_.z = std::clamp(
		panOffset_.z, -kMaximumPanDistance, kMaximumPanDistance);
	return IsFinite(panOffset_);
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
		focusPosition.x + panOffset_.x,
		focusPosition.y + kBaseHeight * zoomScale_,
		focusPosition.z + panOffset_.z - kBaseDepth * zoomScale_,
	};
	if (!IsFinite(position)) {
		return false;
	}

	outputPosition = position;
	return true;
}

} // namespace magnet
