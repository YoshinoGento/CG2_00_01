#pragma once

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmVisualSystem.h"
#include <array>
#include <cmath>
#include <limits>

namespace farm {
struct FarmSoilSurface {
	std::array<Vector3, 16> points{};
	bool valid = false;
	const Vector3& At(int x, int z) const { return points[z * 4 + x]; }
};

inline FarmSoilSurface BuildFarmSoilSurface(const FarmGrid& grid, int index, const FarmVisualSystem& visual) noexcept {
	FarmSoilSurface result;
	const auto data = visual.GetTileVisualData(grid, index);
	if (!data.valid || data.feature != FarmTileFeature::None) { return result; }
	const auto& layout = visual.GetLayout();
	const float outer = (layout.tileSize + layout.tileGap) * 0.5f;
	// Keep existing ridges and crop anchor on the central plateau.
	const float inner = data.halfExtent * 0.86f;
	if (!std::isfinite(outer) || outer - inner < 0.0001f) { return result; }
	const float offsets[4] = {-outer, -inner, inner, outer};
	const int column = index % grid.GetWidth();
	const int row = index / grid.GetWidth();
	const auto sample = [&](int x, int z, float& height) {
		if (x < 0 || z < 0 || x >= grid.GetWidth() || z >= grid.GetHeight()) { return false; }
		const auto neighbor = visual.GetTileVisualData(grid, z * grid.GetWidth() + x);
		if (!neighbor.valid) { return false; }
		height = neighbor.center.y;
		return true;
	};
	for (int z = 0; z < 4; ++z) for (int x = 0; x < 4; ++x) {
		float y = data.center.y;
		const bool edgeX = x == 0 || x == 3;
		const bool edgeZ = z == 0 || z == 3;
		if (edgeX && edgeZ) {
			// Enumerate in global coordinate order so every incident tile gets identical corners.
			const int vertexX = column + (x == 3 ? 1 : 0);
			const int vertexZ = row + (z == 3 ? 1 : 0);
			double sum = 0.0;
			int count = 0;
			for (int dz = -1; dz <= 0; ++dz) for (int dx = -1; dx <= 0; ++dx) {
				float height = 0.0f;
				if (sample(vertexX + dx, vertexZ + dz, height)) { sum += height; ++count; }
			}
			if (count > 0) { y = static_cast<float>(sum / count); }
		} else if (edgeX || edgeZ) {
			float height = 0.0f;
			if (sample(column + (edgeX ? (x == 0 ? -1 : 1) : 0), row + (edgeZ ? (z == 0 ? -1 : 1) : 0), height)) {
				y = static_cast<float>((static_cast<double>(data.center.y) + height) * 0.5);
			}
		}
		const Vector3 point{data.center.x + offsets[x], y, data.center.z + offsets[z]};
		if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) { return {}; }
		result.points[z * 4 + x] = point;
	}
	for (int i = 0; i < 3; ++i) {
		if (result.At(i+1,0).x - result.At(i,0).x < 0.0001f || result.At(0,i+1).z - result.At(0,i).z < 0.0001f) { return {}; }
	}
	result.valid = true;
	return result;
}

// The two planes use the same diagonal as the triangular OBJ footprints.
inline Vector2 FarmSoilPatchSlope(const FarmSoilSurface& surface, int x, int z, bool upper) noexcept {
	const auto& a = surface.At(x, z);
	const auto& b = surface.At(x + 1, z);
	const auto& c = surface.At(x, z + 1);
	const auto& d = surface.At(x + 1, z + 1);
	return upper ? Vector2{(d.y - c.y) / (b.x - a.x), (d.y - b.y) / (c.z - a.z)}
		: Vector2{(b.y - a.y) / (b.x - a.x), (c.y - a.y) / (c.z - a.z)};
}

inline bool TryPickFarmSoilSurface(const FarmSoilSurface& surface, const Vector3& origin, const Vector3& direction, float& distance) noexcept {
	distance = (std::numeric_limits<float>::max)();
	if (!surface.valid) { return false; }
	bool hit = false;
	for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) for (int upper = 0; upper < 2; ++upper) {
		const auto slope = FarmSoilPatchSlope(surface, x, z, upper != 0);
		const auto& anchor = surface.At(x + upper, z + upper);
		const float denominator = direction.y - slope.x * direction.x - slope.y * direction.z;
		if (std::abs(denominator) < 0.000001f) { continue; }
		const float t = (anchor.y + slope.x * (origin.x - anchor.x) + slope.y * (origin.z - anchor.z) - origin.y) / denominator;
		if (!std::isfinite(t) || t < 0.0f || t >= distance) { continue; }
		const auto& a = surface.At(x,z);
		const auto& d = surface.At(x+1,z+1);
		const float u = (origin.x + t * direction.x - a.x) / (d.x - a.x);
		const float v = (origin.z + t * direction.z - a.z) / (d.z - a.z);
		constexpr float tolerance = 0.000001f;
		if (!std::isfinite(u) || !std::isfinite(v) || u < -tolerance || v < -tolerance || u > 1.0f + tolerance || v > 1.0f + tolerance) { continue; }
		if ((!upper && u + v > 1.0f + tolerance) || (upper && u + v < 1.0f - tolerance)) { continue; }
		distance = t;
		hit = true;
	}
	return hit;
}
} // namespace farm
