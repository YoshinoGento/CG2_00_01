#pragma once

#include "math/Struct.h"

namespace farm {
class FarmGrid;
class FarmVisualSystem;

enum class FarmGroundKind { None, Soil, CanalBed, CanalBank };
struct FarmGroundSample {
	bool valid = false;
	float height = 0.0f;
	int tileIndex = -1;
	FarmGroundKind kind = FarmGroundKind::None;
};

// Reads committed CPU geometry only; never retains references or owns render resources.
class FarmTerrainQuerySystem final {
public:
	[[nodiscard]] static FarmGroundSample SampleGround(
		const FarmGrid& grid, const FarmVisualSystem& visual,
		const Vector3& footCenter, const Vector2& halfFootprint) noexcept;
};
} // namespace farm
