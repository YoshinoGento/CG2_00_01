#pragma once

#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmToolActionSystem.h"

class FarmToolSystem;
class Input;

namespace farm {
class FarmGrid;
}

struct FarmInputContext {
	bool keyboardEnabled = true;
	bool cameraDragActive = false;
	bool directToolSelectionEnabled = true;
};

struct FarmInputResult {
	bool navigationInputConsumed = false;
	bool selectionChanged = false;
	bool toolChanged = false;
	bool contentChanged = false;
	bool buySeedRequested = false;
	bool sellSelectedRequested = false;
	bool sellRequested = false;
	FarmToolActionResult toolAction{};
};

struct FarmLockedInputResult {
	bool navigationInputConsumed = false;
	bool actionTriggered = false;
};

class FarmInputSystem final {
public:
	[[nodiscard]] FarmLockedInputResult PollLockedInput(
		const Input& input,
		const FarmInputContext& context) const;
	[[nodiscard]] FarmInputResult Update(
		const Input& input,
		const FarmInputContext& context,
		farm::FarmGrid& grid,
		FarmToolSystem& toolSystem,
		farm::CropType selectedCrop,
		FarmEconomySystem& economySystem,
		FarmToolActionSystem& actionSystem) const;
};
