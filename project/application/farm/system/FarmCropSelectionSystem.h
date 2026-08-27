#pragma once

#include "farm/core/FarmTypes.h"
#include "math/Matrix.h"

class FarmCropSelectionSystem final {
public:
	struct Snapshot {
		farm::CropType selectedCrop = farm::CropType::TestCrop;
	};

	void Initialize() noexcept;
	bool Open(const Vector2& center) noexcept;
	void UpdatePointer(const Vector2& pointer) noexcept;
	bool Confirm() noexcept;
	void Cancel() noexcept;
	bool SetSelectedCrop(farm::CropType crop) noexcept;

	[[nodiscard]] bool IsOpen() const noexcept { return open_; }
	[[nodiscard]] farm::CropType GetSelectedCrop() const noexcept { return selectedCrop_; }
	[[nodiscard]] farm::CropType GetHoveredCrop() const noexcept { return hoveredCrop_; }
	[[nodiscard]] const Vector2& GetCenter() const noexcept { return center_; }
	[[nodiscard]] const Vector2& GetPointer() const noexcept { return pointer_; }
	[[nodiscard]] Snapshot CaptureSnapshot() const noexcept { return { selectedCrop_ }; }
	bool RestoreSnapshot(const Snapshot& snapshot) noexcept;

private:
	farm::CropType selectedCrop_ = farm::CropType::TestCrop;
	farm::CropType hoveredCrop_ = farm::CropType::None;
	Vector2 center_{};
	Vector2 pointer_{};
	bool open_ = false;
};
