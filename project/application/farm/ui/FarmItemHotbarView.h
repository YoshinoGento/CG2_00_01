#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmHUD.h"

namespace farmui {
inline void BuildItemHotbarView(View& view, const FarmHUDViewData& data, bool enabled) {
    if (view.modal || view.terrain || view.observation || view.seedShop || view.harvestDisplay ||
        data.cropPieOpen || data.irrigationPreviewActive) return;
    constexpr std::array<int, 3> tools{0, 1, 3};
    constexpr std::array<Label, 3> labels{Label::Hoe, Label::Water, Label::Harvest};
    constexpr std::array<ItemIcon, 3> icons{ItemIcon::Hoe, ItemIcon::Water, ItemIcon::Harvest};
    constexpr std::array<Label, 3> crops{Label::Carrot, Label::Tomato, Label::Pumpkin};
    if (view.count > view.items.size() - ItemHotbarLayout::slotCount) return;
    view.itemHotbar = true;
    for (std::size_t i = 0; i < tools.size(); ++i) {
        view.Add(labels[i], ItemHotbarLayout::Slot(i), {Action::Tool, tools[i]}, enabled, data.currentToolIndex == tools[i]);
        auto& item = view.items[view.count-1];
        item.icon = icons[i];
    }
    for (std::size_t i = 0; i < crops.size(); ++i) {
        const auto crop = farm::PlayableCrop(static_cast<int>(i));
        const int slot = farm::ToCropSlot(crop);
        const int count = (std::max)(0, data.cropSeedCounts[slot]);
        // An empty packet can still be selected; purchase/plant rules remain in Systems.
        view.Add(crops[i], ItemHotbarLayout::Slot(i+3), {Action::EquipSeed, slot}, enabled,
            data.currentToolIndex == 2 && data.selectedSeedCrop == crop);
        auto& item = view.items[view.count-1];
        item.icon = CropItemIcon(crop, true);
        item.value = count > 999 ? "999+" : std::to_string(count);
    }
}
}
