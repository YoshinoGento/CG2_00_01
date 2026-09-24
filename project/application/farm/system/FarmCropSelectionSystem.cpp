#include "farm/system/FarmCropSelectionSystem.h"

#include <cmath>

namespace {
constexpr float kSelectionDeadZone = 44.0f;

bool IsFinite(const Vector2& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y);
}
}

void FarmCropSelectionSystem::Initialize() noexcept
{
	selectedCrop_ = farm::CropType::Carrot;
	hoveredCrop_ = farm::CropType::None;
	center_ = {};
	pointer_ = {};
	open_ = false;
}

bool FarmCropSelectionSystem::Open(const Vector2& center) noexcept
{
	if (!IsFinite(center)) {
		return false;
	}
	center_ = center;
	pointer_ = center;
	hoveredCrop_ = farm::CropType::None;
	open_ = true;
	return true;
}

void FarmCropSelectionSystem::UpdatePointer(const Vector2& pointer) noexcept
{
	if (!open_ || !IsFinite(pointer)) {
		return;
	}
	pointer_ = pointer;
	const float deltaX = pointer_.x - center_.x;
	const float deltaY = pointer_.y - center_.y;
	if (std::hypot(deltaX, deltaY) < kSelectionDeadZone) {
		hoveredCrop_ = farm::CropType::None;
		return;
	}
	// Three directions match the HUD: left carrot, right tomato, down pumpkin.
	if (deltaY > std::abs(deltaX)) hoveredCrop_ = farm::CropType::Pumpkin;
	else if (std::abs(deltaX) >= std::abs(deltaY))
		hoveredCrop_ = deltaX < 0 ? farm::CropType::Carrot : farm::CropType::Tomato;
	else hoveredCrop_ = farm::CropType::None;
}

bool FarmCropSelectionSystem::Confirm() noexcept
{
	if (!open_) {
		return false;
	}
	open_ = false;
	if (!farm::IsPlantableCrop(hoveredCrop_)) {
		hoveredCrop_ = farm::CropType::None;
		return false;
	}
	const bool changed = selectedCrop_ != hoveredCrop_;
	selectedCrop_ = hoveredCrop_;
	hoveredCrop_ = farm::CropType::None;
	return changed;
}

void FarmCropSelectionSystem::Cancel() noexcept
{
	open_ = false;
	hoveredCrop_ = farm::CropType::None;
}

bool FarmCropSelectionSystem::SetSelectedCrop(farm::CropType crop) noexcept
{
	if (!farm::IsPlantableCrop(crop)) {
		return false;
	}
	selectedCrop_ = crop;
	return true;
}

bool FarmCropSelectionSystem::RestoreSnapshot(const Snapshot& snapshot) noexcept
{
	if (!farm::IsPlantableCrop(snapshot.selectedCrop)) {
		return false;
	}
	selectedCrop_ = snapshot.selectedCrop;
	hoveredCrop_ = farm::CropType::None;
	open_ = false;
	return true;
}
