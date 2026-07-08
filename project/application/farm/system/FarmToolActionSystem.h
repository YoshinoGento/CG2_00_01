#pragma once

#include "farm/system/FarmToolSystem.h"

namespace farm {
class FarmGrid;
}

class FarmToolActionSystem {
public:
	bool ApplyTool(farm::FarmGrid& grid, FarmTool tool);
};
