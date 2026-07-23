#include "farm/system/FarmVisualSystem.h"

#include "3d/LineDrawer.h"
#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <cmath>

namespace farm {
namespace {
constexpr float kRayEpsilon = 0.0001f;
constexpr float kSelectionLift = 0.06f;
constexpr float kSelectionMargin = 0.08f;

void DrawHorizontalRectangle(
	LineDrawer& lineDrawer,
	const Vector3& center,
	float halfExtent,
	const Vector4& color)
{
	const Vector3 topLeft = { center.x - halfExtent, center.y, center.z - halfExtent };
	const Vector3 topRight = { center.x + halfExtent, center.y, center.z - halfExtent };
	const Vector3 bottomRight = { center.x + halfExtent, center.y, center.z + halfExtent };
	const Vector3 bottomLeft = { center.x - halfExtent, center.y, center.z + halfExtent };
	lineDrawer.DrawLine(topLeft, topRight, color);
	lineDrawer.DrawLine(topRight, bottomRight, color);
	lineDrawer.DrawLine(bottomRight, bottomLeft, color);
	lineDrawer.DrawLine(bottomLeft, topLeft, color);
}

Vector4 GetTileColor(const FarmTile& tile)
{
	if (tile.state == FarmTileState::Planted) {
		return IsHarvestReady(tile)
			? Vector4{ 1.0f, 0.78f, 0.12f, 1.0f }
			: Vector4{ 0.25f, 0.88f, 0.32f, 1.0f };
	}
	if (tile.state == FarmTileState::Tilled) {
		return tile.moisture > 0.5f
			? Vector4{ 0.20f, 0.62f, 0.95f, 1.0f }
			: Vector4{ 0.82f, 0.48f, 0.20f, 1.0f };
	}
	return { 0.38f, 0.76f, 0.28f, 1.0f };
}
} // namespace

void FarmVisualSystem::Initialize(const FarmVisualLayout& layout) noexcept
{
	layout_ = layout;
	layout_.tileSize = (std::max)(layout_.tileSize, 0.1f);
	layout_.tileGap = (std::max)(layout_.tileGap, 0.0f);
	layout_.heightStep = (std::max)(layout_.heightStep, 0.0f);
}

Vector3 FarmVisualSystem::GetTileCenter(const FarmGrid& grid, int tileIndex) const noexcept
{
	const FarmTile* tile = grid.GetTile(tileIndex);
	const int width = grid.GetWidth();
	const int height = grid.GetHeight();
	if (tile == nullptr || width <= 0 || height <= 0) {
		return layout_.center;
	}

	const int column = tileIndex % width;
	const int row = tileIndex / width;
	const float pitch = layout_.tileSize + layout_.tileGap;
	return {
		layout_.center.x + (static_cast<float>(column) - static_cast<float>(width - 1) * 0.5f) * pitch,
		layout_.center.y + static_cast<float>(tile->heightLevel) * layout_.heightStep,
		layout_.center.z + (static_cast<float>(row) - static_cast<float>(height - 1) * 0.5f) * pitch,
	};
}

bool FarmVisualSystem::TryPickTile(
	const FarmGrid& grid,
	const Vector3& rayOrigin,
	const Vector3& rayDirection,
	int& outTileIndex) const noexcept
{
	outTileIndex = -1;
	if (grid.GetTileCount() <= 0 || std::abs(rayDirection.y) <= kRayEpsilon) {
		return false;
	}

	float nearestDistance = 3.402823466e+38F;
	const float halfExtent = layout_.tileSize * 0.5f;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const Vector3 center = GetTileCenter(grid, index);
		const float distance = (center.y - rayOrigin.y) / rayDirection.y;
		if (!std::isfinite(distance) || distance < 0.0f || distance >= nearestDistance) {
			continue;
		}

		const Vector3 hit = rayOrigin + rayDirection * distance;
		if (!std::isfinite(hit.x) || !std::isfinite(hit.z)) {
			continue;
		}
		if (std::abs(hit.x - center.x) <= halfExtent &&
			std::abs(hit.z - center.z) <= halfExtent) {
			nearestDistance = distance;
			outTileIndex = index;
		}
	}
	return outTileIndex >= 0;
}

void FarmVisualSystem::Draw(
	const FarmGrid& grid,
	const FarmToolActionResult& selectedAction,
	LineDrawer& lineDrawer) const
{
	const float halfExtent = layout_.tileSize * 0.5f;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile == nullptr) {
			continue;
		}

		const Vector3 center = GetTileCenter(grid, index);
		const Vector4 tileColor = GetTileColor(*tile);
		DrawHorizontalRectangle(lineDrawer, center, halfExtent, tileColor);

		if (tile->state != FarmTileState::Empty) {
			Vector3 moundCenter = center;
			moundCenter.y += 0.02f;
			DrawHorizontalRectangle(lineDrawer, moundCenter, halfExtent * 0.58f, tileColor);
		}
		if (tile->moisture > 0.05f) {
			const Vector4 moistureColor = { 0.18f, 0.68f, 1.0f, 1.0f };
			const float moistureExtent = halfExtent * std::clamp(tile->moisture, 0.15f, 0.85f);
			Vector3 moistureCenter = center;
			moistureCenter.y += 0.03f;
			lineDrawer.DrawLine(
				{ moistureCenter.x - moistureExtent, moistureCenter.y, moistureCenter.z },
				{ moistureCenter.x + moistureExtent, moistureCenter.y, moistureCenter.z },
				moistureColor);
		}

		if (tile->state == FarmTileState::Planted) {
			const float growth = std::clamp(tile->growth, 0.0f, 1.0f);
			const float stemHeight = 0.18f + growth * 0.72f;
			const Vector3 stemBase = { center.x, center.y + 0.03f, center.z };
			const Vector3 stemTop = { center.x, stemBase.y + stemHeight, center.z };
			const Vector4 stemColor = { 0.18f, 0.92f, 0.25f, 1.0f };
			lineDrawer.DrawLine(stemBase, stemTop, stemColor);
			if (growth >= 0.25f) {
				const float leafWidth = 0.10f + growth * 0.22f;
				const float leafY = stemBase.y + stemHeight * 0.55f;
				lineDrawer.DrawLine(
					{ center.x, leafY, center.z },
					{ center.x - leafWidth, leafY + 0.08f, center.z },
					stemColor);
				lineDrawer.DrawLine(
					{ center.x, leafY + 0.06f, center.z },
					{ center.x + leafWidth, leafY + 0.14f, center.z },
					stemColor);
			}
			if (IsHarvestReady(*tile)) {
				lineDrawer.DrawWireCube(
					{ stemTop.x, stemTop.y + 0.12f, stemTop.z },
					0.24f,
					{ 1.0f, 0.72f, 0.08f, 1.0f });
			}
		}

		if (index == grid.GetSelectedIndex()) {
			const Vector4 selectionColor = selectedAction.Succeeded()
				? Vector4{ 0.15f, 1.0f, 0.35f, 1.0f }
				: Vector4{ 1.0f, 0.22f, 0.12f, 1.0f };
			Vector3 selectionCenter = center;
			selectionCenter.y += kSelectionLift;
			DrawHorizontalRectangle(
				lineDrawer, selectionCenter, halfExtent + kSelectionMargin, selectionColor);
			DrawHorizontalRectangle(
				lineDrawer, selectionCenter, halfExtent + kSelectionMargin * 1.75f, selectionColor);
		}
	}
}

} // namespace farm
