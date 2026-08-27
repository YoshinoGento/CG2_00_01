#include "farm/system/FarmInputSystem.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolSystem.h"
#include "io/Input.h"

FarmLockedInputResult FarmInputSystem::PollLockedInput(
	const Input& input,
	const FarmInputContext& context) const
{
	FarmLockedInputResult result{};
	if (!context.keyboardEnabled) {
		return result;
	}

	result.navigationInputConsumed =
		input.PushKey(InputKey::ArrowUp) ||
		input.PushKey(InputKey::ArrowDown) ||
		input.PushKey(InputKey::ArrowLeft) ||
		input.PushKey(InputKey::ArrowRight);
	result.actionTriggered =
		input.TriggerKey(InputKey::ArrowUp) ||
		input.TriggerKey(InputKey::ArrowDown) ||
		input.TriggerKey(InputKey::ArrowLeft) ||
		input.TriggerKey(InputKey::ArrowRight) ||
		input.TriggerKey(InputKey::PageUp) ||
		input.TriggerKey(InputKey::PageDown) ||
		input.TriggerKey(InputKey::Num1) ||
		input.TriggerKey(InputKey::Num2) ||
		input.TriggerKey(InputKey::Num3) ||
		input.TriggerKey(InputKey::Num4) ||
		input.TriggerKey(InputKey::Q) ||
		input.TriggerKey(InputKey::E) ||
		input.TriggerKey(InputKey::C) ||
		input.TriggerKey(InputKey::B) ||
		input.TriggerKey(InputKey::V) ||
		input.TriggerKey(InputKey::Enter) ||
		input.TriggerKey(InputKey::F);
	return result;
}

FarmInputResult FarmInputSystem::Update(
	const Input& input,
	const FarmInputContext& context,
	farm::FarmGrid& grid,
	FarmToolSystem& toolSystem,
	farm::CropType selectedCrop,
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
			grid, toolSystem.GetCurrentTool(), selectedCrop, economySystem);
		result.contentChanged |= result.toolAction.Succeeded();
	}
	result.buySeedRequested = input.TriggerKey(InputKey::B);
	result.sellSelectedRequested = input.TriggerKey(InputKey::V);
	result.sellRequested = input.TriggerKey(InputKey::F);
	return result;
}
