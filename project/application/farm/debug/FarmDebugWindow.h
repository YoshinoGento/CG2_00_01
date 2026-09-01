#pragma once

#include "farm/system/FarmToolSystem.h"

namespace farm {
class FarmGrid;
}

class FarmToolActionSystem;

namespace farm {

class FarmDebugWindow {
public:
	void Draw(
		FarmGrid& grid,
		FarmToolActionSystem& toolActionSystem);
};

} // namespace farm
