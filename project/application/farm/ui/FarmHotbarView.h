#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmHUD.h"

namespace farmui {
inline void BuildHotbarView(View& view, const FarmHUDViewData& state, bool enabled) {
    view.hotbarVisible = true;
    constexpr std::array toolLabels{Label::Hoe, Label::Water, Label::Seed, Label::Harvest};
    constexpr std::array toolIcons{HUDIcon::Hoe, HUDIcon::WateringCan, HUDIcon::SeedBag, HUDIcon::HarvestBasket};
    for (int i = 0; i < 4; ++i)
        view.IconButton(toolLabels[i], toolIcons[i], HUDLayout::ToolSlot(i), {Action::Tool, i},
            enabled, state.currentToolIndex == i);
    constexpr std::array cropLabels{Label::Carrot, Label::Tomato, Label::Pumpkin};
    constexpr std::array cropIcons{HUDIcon::SeedCarrot, HUDIcon::SeedTomato, HUDIcon::SeedPumpkin};
    for (int i = 0; i < farm::kPlayableCropCount; ++i) {
        const int slot = i + 1;
        view.IconButton(cropLabels[i], cropIcons[i], HUDLayout::SeedSlot(i), {Action::Crop, slot},
            enabled, state.selectedSeedCrop == farm::CropTypeFromSlot(slot),
            std::to_string((std::max)(0, state.cropSeedCounts[slot])));
    }
}
}
