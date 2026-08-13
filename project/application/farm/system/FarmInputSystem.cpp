#include "farm/system/FarmInputSystem.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolSystem.h"
#include "io/Input.h"

FarmInputResult FarmInputSystem::Update(
	const Input& input,
	const FarmInputContext& context,
	farm::FarmGrid& grid,
	FarmToolSystem& toolSystem,
	FarmEconomySystem& economySystem,
	FarmToolActionSystem& actionSystem) const
{
	FarmInputResult result{};
	if (!context.keyboardEnabled) {
		return result;
	}

	result.navigationInputConsumed =
		input.PushKey(InputKey::ArrowUp) ||
		input.PushKey(InputKey::ArrowDown) ||
		input.PushKey(InputKey::ArrowLeft) ||
		input.PushKey(InputKey::ArrowRight);

	const int previousSelection = grid.GetSelectedIndex();
	if (input.TriggerKey(InputKey::ArrowUp)) {
		grid.MoveSelection(0, 1);
	} else if (input.TriggerKey(InputKey::ArrowDown)) {
		grid.MoveSelection(0, -1);
	} else if (input.TriggerKey(InputKey::ArrowLeft)) {
		grid.MoveSelection(-1, 0);
	} else if (input.TriggerKey(InputKey::ArrowRight)) {
		grid.MoveSelection(1, 0);
	}
	result.selectionChanged = previousSelection != grid.GetSelectedIndex();

	if (input.TriggerKey(InputKey::PageUp)) {
		result.contentChanged |= actionSystem.RaiseSelectedTile(grid);
	}
	if (input.TriggerKey(InputKey::PageDown)) {
		result.contentChanged |= actionSystem.LowerSelectedTile(grid);
	}

	const FarmTool previousTool = toolSystem.GetCurrentTool();
	if (context.directToolSelectionEnabled) {
		if (input.TriggerKey(InputKey::Num1)) {
			toolSystem.SetTool(FarmTool::Hoe);
		} else if (input.TriggerKey(InputKey::Num2)) {
			toolSystem.SetTool(FarmTool::Water);
		} else if (input.TriggerKey(InputKey::Num3)) {
			toolSystem.SetTool(FarmTool::Seed);
		} else if (input.TriggerKey(InputKey::Num4)) {
			toolSystem.SetTool(FarmTool::Harvest);
		}
	}
	if (!context.cameraDragActive) {
		if (input.TriggerKey(InputKey::E)) {
			toolSystem.SelectNextTool();
		} else if (input.TriggerKey(InputKey::Q)) {
			toolSystem.SelectPreviousTool();
		}
	}
	result.toolChanged = previousTool != toolSystem.GetCurrentTool();

	if (input.TriggerKey(InputKey::Enter)) {
		result.toolAction = actionSystem.ApplyToolDetailed(
			grid, toolSystem.GetCurrentTool(), &economySystem);
		result.contentChanged |= result.toolAction.Succeeded();
	}
	result.sellRequested = input.TriggerKey(InputKey::F);
	return result;
}
