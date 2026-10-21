#pragma once
#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
inline void BuildCameraView(View& view, bool followsPlayer, bool playerAvailable) {
    view.IconButton(Label::Overview, HUDIcon::Overview, HUDLayout::UtilitySlot(2), {Action::Overview}, true, !followsPlayer);
    if (playerAvailable)
        view.IconButton(Label::Follow, HUDIcon::Follow, HUDLayout::UtilitySlot(3), {Action::Follow}, true, followsPlayer);
}
}
