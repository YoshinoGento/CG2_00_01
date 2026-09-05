#pragma once

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/render/FarmSoilSurface.h"
#include <array>
#include <cmath>

namespace farm {
enum class FarmMeshShape { Box, TriangleLower, TriangleUpper };
enum class FarmMeshSurface { None, CanalBed, CanalBank };
struct FarmMeshPart {
	Vector3 position{};
	Vector3 scale{1.0f, 1.0f, 1.0f}; // Unit box spans [-1,1]; scale is half-size.
	Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
	bool water = false;
	Vector2 slope{}; // dY/dX and dY/dZ before rotation, applied after scale.
	FarmMeshShape shape = FarmMeshShape::Box;
	FarmMeshSurface surface = FarmMeshSurface::None;
};
struct FarmTileMeshParts {
	// Soil: five planar + eight triangular patches, earth/cap each, plus three ridges.
	std::array<FarmMeshPart, 32> parts{};
	std::size_t count = 0;
};

// Pure presentation conversion, usable by CPU tests without allocating GPU resources.
inline FarmTileMeshParts BuildFarmTileMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& visualSystem) noexcept {
	constexpr float kEarthDepth = 0.24f;
	constexpr float kTopThickness = 0.04f;
	constexpr float kWaterHalfThickness = 0.005f;
	constexpr float kRimFreeboard = 0.045f;
	const Vector4 waterColor{0.12f, 0.51f, 0.69f, 1.0f};
	FarmTileMeshParts result;
	const auto visual = visualSystem.GetTileVisualData(grid, index);
	const auto* tile = grid.GetTile(index);
	if (!visual.valid || !tile) { return result; }
	const auto& layout = visualSystem.GetLayout();
	const float cellHalf = (layout.tileSize + layout.tileGap) * 0.5f;
	const float groundBottom = layout.center.y - kEarthDepth;
	const float half = visual.halfExtent;
	const auto add = [&](Vector3 position, Vector3 scale, Vector4 color, bool water = false, Vector2 slope = {}, FarmMeshShape shape = FarmMeshShape::Box, FarmMeshSurface surface = FarmMeshSurface::None) {
		if (result.count >= result.parts.size() || !std::isfinite(position.x) ||
			!std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(scale.x) ||
			!std::isfinite(scale.y) || !std::isfinite(scale.z) || !std::isfinite(slope.x) || !std::isfinite(slope.y) ||
			scale.x < 0.0001f || scale.y < 0.0001f || scale.z < 0.0001f) { return; }
		result.parts[result.count++] = {position, scale, color, water, slope, shape, surface};
	};
	if (visual.feature == FarmTileFeature::None) {
		const float wetness = visual.soilWetness;
		const Vector4 soil = tile->state == FarmTileState::Empty
			? Vector4{0.27f - 0.08f * wetness, 0.57f - 0.15f * wetness, 0.13f, 1.0f}
			: Vector4{0.52f - 0.28f * wetness, 0.32f - 0.18f * wetness, 0.16f - 0.09f * wetness, 1.0f};
		const Vector4 earth{0.43f - 0.16f * wetness, 0.24f - 0.10f * wetness, 0.10f, 1.0f};
		const auto surface = BuildFarmSoilSurface(grid, index, visualSystem);
		if (!surface.valid) { return {}; }
		bool flat = true;
		for (const auto& point : surface.points) { flat &= std::abs(point.y - visual.center.y) < 0.000001f; }
		if (flat) {
			const float bodyTop = visual.center.y - kTopThickness;
			add({visual.center.x, (groundBottom + bodyTop) * 0.5f, visual.center.z},
				{cellHalf, (bodyTop - groundBottom) * 0.5f, cellHalf}, earth);
			add({visual.center.x, visual.center.y - kTopThickness * 0.5f, visual.center.z},
				{cellHalf, kTopThickness * 0.5f, cellHalf}, soil);
		} else {
			// Each triangle retains a buried earth body; no high full-tile block hides the ramp.
			const float bodyHalfDepth = (visual.center.y - groundBottom + 2.0f * layout.heightStep) * 0.5f;
			for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) {
				const auto& a = surface.At(x,z);
				const auto& b = surface.At(x+1,z);
				const auto& c = surface.At(x,z+1);
				const auto& d = surface.At(x+1,z+1);
				const bool planar = std::abs(a.y + d.y - b.y - c.y) < 0.000001f;
				for (int upper = 0; upper < (planar ? 1 : 2); ++upper) {
					const auto slope = FarmSoilPatchSlope(surface,x,z,upper != 0);
					const auto& anchor = upper ? d : a;
					const float centerX = (a.x + d.x) * 0.5f;
					const float centerZ = (a.z + d.z) * 0.5f;
					const float top = anchor.y + slope.x * (centerX-anchor.x) + slope.y * (centerZ-anchor.z);
					const auto shape = planar ? FarmMeshShape::Box : (upper ? FarmMeshShape::TriangleUpper : FarmMeshShape::TriangleLower);
					add({centerX, top - kTopThickness - bodyHalfDepth, centerZ},
						{(d.x-a.x)*0.5f,bodyHalfDepth,(d.z-a.z)*0.5f},earth,false,slope,shape);
					add({centerX, top - kTopThickness*0.5f, centerZ},
						{(d.x-a.x)*0.5f,kTopThickness*0.5f,(d.z-a.z)*0.5f},soil,false,slope,shape);
				}
			}
		}
		if (tile->state != FarmTileState::Empty) {
			for (int ridge = -1; ridge <= 1; ++ridge) {
				add({visual.center.x + ridge * half * 0.52f, visual.center.y + 0.012f, visual.center.z},
					{half * 0.12f, 0.012f, half * 0.83f}, soil);
			}
		}
		return result;
	}
	const float bottom = visual.center.y + layout.waterBottomOffset;
	const float rimHeight = layout.waterMaxDepth + kRimFreeboard;
	const Vector4 stone{0.45f, 0.34f, 0.20f, 1.0f};
	const int width = grid.GetWidth();
	const int column = index % width;
	const int row = index / width;
	const int neighbors[4] = {column > 0 ? index-1 : -1, column+1 < width ? index+1 : -1,
		row > 0 ? index-width : -1, row+1 < grid.GetHeight() ? index+width : -1};
	std::array<FarmTileVisualData, 4> neighborData{};
	std::array<float, 4> bedSlope{};
	const float inner = visual.waterHalfExtent;
	const float run = cellHalf - inner;
	if (!std::isfinite(run) || run < 0.0001f) { return {}; }
	for (int edge = 0; edge < 4; ++edge) {
		neighborData[edge] = visualSystem.GetTileVisualData(grid, neighbors[edge]);
		const auto& neighbor = neighborData[edge];
		if (neighbor.valid && neighbor.feature != FarmTileFeature::None) {
			// Both halves meet at the mean height, regardless of traversal order.
			bedSlope[edge] = (neighbor.center.y - visual.center.y) * 0.5f / run;
		}
	}
	const float offsets[3] = {-(cellHalf + inner) * 0.5f, 0.0f, (cellHalf + inner) * 0.5f};
	const float extents[3] = {run * 0.5f, inner, run * 0.5f};
	const float xSlopes[3] = {-bedSlope[0], 0.0f, bedSlope[1]};
	const float zSlopes[3] = {-bedSlope[2], 0.0f, bedSlope[3]};
	const float xRise[3] = {bedSlope[0] * run * 0.5f, 0.0f, bedSlope[1] * run * 0.5f};
	const float zRise[3] = {bedSlope[2] * run * 0.5f, 0.0f, bedSlope[3] * run * 0.5f};
	// Remove the old full-height bed: it would occlude a descending water ramp.
	// Deep overlapping undersides stay below the field; each top section has one owner.
	const float bodyHalfDepth = (bottom - groundBottom + 2.0f * layout.heightStep) * 0.5f;
	for (int z = 0; z < 3; ++z) {
		for (int x = 0; x < 3; ++x) {
			add({visual.center.x + offsets[x], bottom + xRise[x] + zRise[z] - bodyHalfDepth, visual.center.z + offsets[z]},
				{extents[x], bodyHalfDepth, extents[z]}, {0.43f, 0.24f, 0.10f, 1.0f}, false, {xSlopes[x], zSlopes[z]}, FarmMeshShape::Box, FarmMeshSurface::CanalBed);
		}
	}
	if (visual.showWaterSurface) {
		add({visual.center.x, visual.waterSurfaceCenter.y - kWaterHalfThickness, visual.center.z},
			{inner, kWaterHalfThickness, inner}, waterColor, true);
	}
	for (int edge = 0; edge < 4; ++edge) {
		const auto& neighbor = neighborData[edge];
		const bool open = neighbor.valid && neighbor.feature != FarmTileFeature::None;
		const float sign = edge % 2 == 0 ? -1.0f : 1.0f;
		const bool alongX = edge < 2;
		if (!open) {
			const float rimWidth = half * 0.14f;
			// Split side banks with the bed so their ends follow the same transition.
			for (int section = 0; section < 3; ++section) {
				const float rise = alongX ? zRise[section] : xRise[section];
				const Vector2 slope = alongX ? Vector2{0.0f, zSlopes[section]} : Vector2{xSlopes[section], 0.0f};
				add({visual.center.x + (alongX ? sign * (cellHalf - rimWidth) : offsets[section]), bottom + rise + rimHeight * 0.5f,
					visual.center.z + (alongX ? offsets[section] : sign * (cellHalf - rimWidth))},
					{alongX ? rimWidth : extents[section], rimHeight * 0.5f, alongX ? extents[section] : rimWidth}, stone, false, slope, FarmMeshShape::Box, FarmMeshSurface::CanalBank);
			}
			continue;
		}
		if (!visual.showWaterSurface || !neighbor.showWaterSurface) { continue; }
		const float armHalf = run * 0.5f;
		const float armOffset = (cellHalf + inner) * 0.5f;
		const float boundaryRise = (neighbor.waterSurfaceCenter.y - visual.waterSurfaceCenter.y) * 0.5f;
		const float slope = sign * boundaryRise / run;
		add({visual.center.x + (alongX ? sign * armOffset : 0.0f), visual.waterSurfaceCenter.y + boundaryRise * 0.5f - kWaterHalfThickness,
			visual.center.z + (alongX ? 0.0f : sign * armOffset)},
			{alongX ? armHalf : inner, kWaterHalfThickness, alongX ? inner : armHalf}, waterColor, true,
			alongX ? Vector2{slope, 0.0f} : Vector2{0.0f, slope});
	}
	return result;
}
} // namespace farm
