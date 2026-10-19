#pragma once
#include "farm/core/FarmTypes.h"
#include "math/Struct.h"
#include <cstddef>

namespace farmui {
enum class ItemIcon { None = -1, Hoe, Water, Seed, Harvest, Carrot, Tomato, Pumpkin,
    SeedCarrot, SeedTomato, SeedPumpkin, Count };
inline constexpr const char* kItemIconPath = "Resources/ui/farm_item_icons.png";
inline constexpr float kItemIconCell = 128;
inline constexpr int kItemIconColumns = 4;
[[nodiscard]] constexpr bool ValidIcon(ItemIcon icon) noexcept {
    return icon >= ItemIcon::Hoe && icon < ItemIcon::Count;
}
[[nodiscard]] constexpr Vector2 ItemIconOrigin(ItemIcon icon) noexcept {
    if (!ValidIcon(icon)) return {};
    const int index = static_cast<int>(icon);
    return {static_cast<float>(index % kItemIconColumns) * kItemIconCell,
        static_cast<float>(index / kItemIconColumns) * kItemIconCell};
}
[[nodiscard]] constexpr ItemIcon CropItemIcon(farm::CropType crop, bool seed = false) noexcept {
    switch (crop) {
    case farm::CropType::Carrot: return seed ? ItemIcon::SeedCarrot : ItemIcon::Carrot;
    case farm::CropType::Tomato: return seed ? ItemIcon::SeedTomato : ItemIcon::Tomato;
    case farm::CropType::Pumpkin: return seed ? ItemIcon::SeedPumpkin : ItemIcon::Pumpkin;
    default: return ItemIcon::None;
    }
}
}
