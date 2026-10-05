#pragma once

#include "title/TitlePresentationSystem.h"
#include "farm/render/FarmMeshLayout.h"

namespace title {
// Shared crop geometry is adapted only for this title; no saved tile is modified.
inline farm::FarmCropMeshParts BuildTitleCropParts(farm::FarmTileVisualData visual,
    const CropFrame& frame) noexcept {
    if (!frame.visible || !std::isfinite(frame.growth) || frame.growth < 0 || frame.growth > 1) return {};
    farm::FarmTile staged;
    staged.state = farm::FarmTileState::Planted;
    staged.crop = visual.crop;
    staged.growth = frame.growth;
    visual.cropStage = farm::GetCropGrowthStage(staged);
    return farm::BuildFarmCropMeshParts(visual, frame.growth);
}
} // namespace title
