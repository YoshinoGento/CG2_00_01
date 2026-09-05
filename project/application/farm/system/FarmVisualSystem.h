#pragma once

#include "farm/system/FarmToolActionSystem.h"
#include "math/Struct.h"

#include <vector>

class LineDrawer;

namespace farm {

class FarmGrid;
class FarmIrrigationSystem;

struct FarmVisualLayout {
	Vector3 center = { 0.0f, 0.05f, 8.0f };
	float tileSize = 1.25f;
	float tileGap = 0.18f;
	float heightStep = 0.18f;
};

// Read-only world representation shared by rendering and viewport picking.
class FarmVisualSystem final {
public:
	void Initialize(const FarmVisualLayout& layout) noexcept;

	[[nodiscard]] Vector3 GetTileCenter(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] bool TryPickTile(
		const FarmGrid& grid,
		const Vector3& rayOrigin,
		const Vector3& rayDirection,
		int& outTileIndex) const noexcept;

	void Draw(
		const FarmGrid& grid,
		const FarmIrrigationSystem& irrigationSystem,
		const FarmToolActionResult& selectedAction,
		LineDrawer& lineDrawer,
		const std::vector<int>* irrigationPreviewChangedTiles = nullptr) const;

	[[nodiscard]] const FarmVisualLayout& GetLayout() const noexcept { return layout_; }

private:
	FarmVisualLayout layout_{};
};

} // namespace farm
