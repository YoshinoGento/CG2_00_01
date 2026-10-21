#pragma once
#include "math/Struct.h"

namespace farmui {
enum class HUDIcon : int {
    None = -1, Hoe, WateringCan, SeedBag, HarvestBasket, Carrot, Tomato, Pumpkin,
    WaterDrop, Sprout, Calendar, Coin, Terrain, Pause, Play, Overview, Follow,
    Menu, Shop, SeedCarrot, SeedTomato, SeedPumpkin, Rank, Canal, Source,
    Back, Check, Undo, Redo, Plus, Minus, Close, Inventory,
};
inline constexpr const char* kHUDIconAtlas = "Resources/ui/farm_hud_icons.png";
inline constexpr float kHUDIconSize = 64.0f;
[[nodiscard]] constexpr bool IsValidIcon(HUDIcon icon) noexcept {
    return static_cast<int>(icon) >= 0 && static_cast<int>(icon) < 32;
}
[[nodiscard]] constexpr Vector2 IconUV(HUDIcon icon) noexcept {
    const int index = IsValidIcon(icon) ? static_cast<int>(icon) : 0;
    return {static_cast<float>(index % 8) * kHUDIconSize,
        static_cast<float>(index / 8) * kHUDIconSize};
}
}
