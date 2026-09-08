#pragma once

#ifdef USE_IMGUI
#include "farm/debug/FarmDebugWindow.h"
#endif

namespace farm {
class FarmGrid;
}

class FarmToolActionSystem;

namespace farm {

class FarmDebugEditorWindow {
public:
	void LoadSettings();
	void SaveSettings();
	void Draw(
		FarmGrid& grid,
		FarmToolActionSystem& toolActionSystem);

private:
#ifdef USE_IMGUI
	bool applyWindowPlacement_ = false;
	bool showFarmDebugWindow_ = true;
	float farmEditorWindowX_ = 20.0f;
	float farmEditorWindowY_ = 40.0f;
	float farmEditorWindowWidth_ = 360.0f;
	float farmEditorWindowHeight_ = 300.0f;
	FarmDebugWindow farmDebugWindow_;
#endif
};

} // namespace farm
