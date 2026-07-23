#include "farm/system/FarmToolSystem.h"

namespace {
	// BugNet remains reserved until its gameplay action is implemented.
	constexpr int kSelectableToolCount = 4;

	int ToToolIndex(FarmTool tool)
	{
		return static_cast<int>(tool);
	}

	FarmTool ToFarmTool(int index)
	{
		switch (index) {
		case 0:
			return FarmTool::Hoe;
		case 1:
			return FarmTool::Water;
		case 2:
			return FarmTool::Seed;
		case 3:
			return FarmTool::Harvest;
		case 4:
			return FarmTool::BugNet;
		default:
			return FarmTool::Hoe;
		}
	}
}

void FarmToolSystem::Initialize()
{
	currentTool_ = FarmTool::Hoe;
}

void FarmToolSystem::SetTool(FarmTool tool)
{
	currentTool_ = tool;
}

void FarmToolSystem::SelectNextTool()
{
	const int currentIndex = ToToolIndex(currentTool_);
	const int nextIndex = currentIndex >= kSelectableToolCount - 1
		? 0 : currentIndex + 1;
	currentTool_ = ToFarmTool(nextIndex);
}

void FarmToolSystem::SelectPreviousTool()
{
	const int currentIndex = ToToolIndex(currentTool_);
	const int previousIndex = currentIndex <= 0 || currentIndex >= kSelectableToolCount
		? kSelectableToolCount - 1 : currentIndex - 1;
	currentTool_ = ToFarmTool(previousIndex);
}

const char* FarmToolSystem::GetCurrentToolName() const
{
	switch (currentTool_) {
	case FarmTool::Hoe:
		return "Hoe";
	case FarmTool::Water:
		return "Water";
	case FarmTool::Seed:
		return "Seed";
	case FarmTool::Harvest:
		return "Harvest";
	case FarmTool::BugNet:
		return "BugNet";
	default:
		return "Unknown";
	}
}
