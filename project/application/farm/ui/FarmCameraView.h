#pragma once
#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
inline void BuildCameraView(View& view, bool followsPlayer, bool playerAvailable) {
    view.Add(Label::Overview, {1040, 428, 216, 44}, {Action::Overview});
    view.Add(Label::Follow, {1040, 480, 216, 44}, {Action::Follow}, playerAvailable, followsPlayer);
}
}
