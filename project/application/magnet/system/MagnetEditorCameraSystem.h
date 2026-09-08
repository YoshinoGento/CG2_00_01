#pragma once

#include "math/Struct.h"

namespace magnet {

// Owns the bounded camera distance used only by the magnet stage editor.
class MagnetEditorCameraSystem final {
public:
	void Reset() noexcept;
	[[nodiscard]] bool ApplyWheelDelta(float wheelDelta) noexcept;
	[[nodiscard]] bool TryCalculatePosition(
		const Vector3& focusPosition,
		Vector3& outputPosition) const noexcept;

	[[nodiscard]] float GetZoomScale() const noexcept { return zoomScale_; }

	static constexpr float kMinimumZoomScale = 0.45f;
	static constexpr float kMaximumZoomScale = 3.0f;

private:
	static constexpr float kDefaultZoomScale = 1.0f;
	static constexpr float kWheelSensitivity = 0.16f;
	static constexpr float kMaximumWheelStepsPerFrame = 8.0f;
	static constexpr float kBaseHeight = 9.0f;
	static constexpr float kBaseDepth = 13.0f;

	float zoomScale_ = kDefaultZoomScale;
};

} // namespace magnet
