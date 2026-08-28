#include "farm/system/FarmVisualSystem.h"

#include "3d/LineDrawer.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"

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

Vector4 GetTileColor(
	const FarmTile& tile,
	int tileIndex,
	const FarmIrrigationSystem& irrigationSystem)
{
	if (tile.feature == FarmTileFeature::Canal) {
		return irrigationSystem.IsSupplied(tileIndex)
			? Vector4{ 0.08f, 0.78f, 1.0f, 1.0f }
			: Vector4{ 0.10f, 0.32f, 0.52f, 1.0f };
	}
	if (tile.feature == FarmTileFeature::WaterSource) {
		return { 0.16f, 0.92f, 1.0f, 1.0f };
	}
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

bool HasDevelopedLeaves(FarmCropGrowthStage stage) noexcept
{
	return stage == FarmCropGrowthStage::Growing ||
		stage == FarmCropGrowthStage::AlmostReady ||
		stage == FarmCropGrowthStage::Ready;
}

bool HasVisibleCropBody(FarmCropGrowthStage stage) noexcept
{
	return stage == FarmCropGrowthStage::AlmostReady ||
		stage == FarmCropGrowthStage::Ready;
}

void DrawCropSilhouette(
	LineDrawer& lineDrawer,
	const FarmTile& tile,
	const Vector3& center)
{
	const FarmCropGrowthStage stage = GetCropGrowthStage(tile);
	if (stage == FarmCropGrowthStage::None) {
		return;
	}

	const float growth = std::clamp(tile.growth, 0.0f, 1.0f);
	const float stemHeight = 0.16f + growth * 0.68f;
	const Vector3 stemBase = { center.x, center.y + 0.03f, center.z };
	const Vector3 stemTop = { center.x, stemBase.y + stemHeight, center.z };
	const Vector4 stemColor = tile.crop == CropType::Carrot
		? Vector4{ 0.12f, 0.72f, 0.24f, 1.0f }
		: Vector4{ 0.34f, 0.94f, 0.30f, 1.0f };
	lineDrawer.DrawLine(stemBase, stemTop, stemColor);

	if (HasDevelopedLeaves(stage)) {
		const float leafWidth = 0.10f + growth * 0.22f;
		const float leafY = stemBase.y + stemHeight * 0.58f;
		lineDrawer.DrawLine(
			{ center.x, leafY, center.z },
			{ center.x - leafWidth, leafY + 0.08f, center.z },
			stemColor);
		lineDrawer.DrawLine(
			{ center.x, leafY + 0.05f, center.z },
			{ center.x + leafWidth, leafY + 0.13f, center.z },
			stemColor);
		lineDrawer.DrawLine(
			{ center.x, leafY + 0.02f, center.z },
			{ center.x, leafY + 0.10f, center.z + leafWidth },
			stemColor);
	}

	if (HasVisibleCropBody(stage)) {
		if (tile.crop == CropType::Carrot) {
			const Vector4 rootColor = { 1.0f, 0.40f, 0.08f, 1.0f };
			const float shoulderWidth = 0.10f + growth * 0.06f;
			const float shoulderY = center.y + 0.17f;
			const Vector3 rootTip = { center.x, center.y + 0.03f, center.z };
			lineDrawer.DrawLine(
				{ center.x - shoulderWidth, shoulderY, center.z }, rootTip, rootColor);
			lineDrawer.DrawLine(
				{ center.x + shoulderWidth, shoulderY, center.z }, rootTip, rootColor);
			lineDrawer.DrawLine(
				{ center.x, shoulderY, center.z - shoulderWidth }, rootTip, rootColor);
			lineDrawer.DrawLine(
				{ center.x, shoulderY, center.z + shoulderWidth }, rootTip, rootColor);
		} else {
			const float bulbRadius = 0.10f + growth * 0.07f;
			lineDrawer.DrawWireSphere(
				{ center.x, center.y + bulbRadius, center.z },
				bulbRadius,
				{ 0.92f, 0.48f, 0.78f, 1.0f },
				8);
		}
	}

	if (stage == FarmCropGrowthStage::Ready) {
		lineDrawer.DrawWireCube(
			{ stemTop.x, stemTop.y + 0.12f, stemTop.z },
			0.24f,
			{ 1.0f, 0.72f, 0.08f, 1.0f });
	}
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
	const FarmIrrigationSystem& irrigationSystem,
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
		const Vector4 tileColor = GetTileColor(*tile, index, irrigationSystem);
		DrawHorizontalRectangle(lineDrawer, center, halfExtent, tileColor);
		if (tile->feature == FarmTileFeature::Canal) {
			Vector3 canalCenter = center;
			canalCenter.y += 0.025f;
			const Vector4 flowColor = irrigationSystem.IsSupplied(index)
				? Vector4{ 0.45f, 0.94f, 1.0f, 1.0f }
				: Vector4{ 0.20f, 0.48f, 0.66f, 1.0f };
			DrawHorizontalRectangle(
				lineDrawer, canalCenter, halfExtent * 0.62f,
				flowColor);
			lineDrawer.DrawLine(
				{ canalCenter.x - halfExtent * 0.45f, canalCenter.y, canalCenter.z },
				{ canalCenter.x + halfExtent * 0.45f, canalCenter.y, canalCenter.z },
				flowColor);
		} else if (tile->feature == FarmTileFeature::WaterSource) {
			Vector3 sourceCenter = center;
			sourceCenter.y += 0.18f;
			lineDrawer.DrawWireSphere(
				sourceCenter,
				halfExtent * 0.28f,
				{ 0.20f, 0.94f, 1.0f, 1.0f },
				12);
			lineDrawer.DrawLine(
				{ sourceCenter.x, center.y + 0.02f, sourceCenter.z },
				{ sourceCenter.x, sourceCenter.y + halfExtent * 0.58f, sourceCenter.z },
				{ 0.72f, 0.98f, 1.0f, 1.0f });
		}

		if (tile->feature == FarmTileFeature::None && tile->state != FarmTileState::Empty) {
			Vector3 moundCenter = center;
			moundCenter.y += 0.02f;
			DrawHorizontalRectangle(lineDrawer, moundCenter, halfExtent * 0.58f, tileColor);
		}
		if (tile->feature == FarmTileFeature::None && tile->moisture > 0.05f) {
			const Vector4 moistureColor = { 0.18f, 0.68f, 1.0f, 1.0f };
			const float moistureExtent = halfExtent * std::clamp(tile->moisture, 0.15f, 0.85f);
			Vector3 moistureCenter = center;
			moistureCenter.y += 0.03f;
			lineDrawer.DrawLine(
				{ moistureCenter.x - moistureExtent, moistureCenter.y, moistureCenter.z },
				{ moistureCenter.x + moistureExtent, moistureCenter.y, moistureCenter.z },
				moistureColor);
		}

		if (tile->feature == FarmTileFeature::None) {
			DrawCropSilhouette(lineDrawer, *tile, center);
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
