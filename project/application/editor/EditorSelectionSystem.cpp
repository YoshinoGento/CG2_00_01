#include "editor/EditorSelectionSystem.h"

namespace editor {

bool EditorSelectionSystem::SelectFarmTile(int index, uint64_t generation, int tileCount) noexcept {
	if (index < 0 || index >= tileCount || generation == 0) {
		return false;
	}
	selection_ = { EditorSelectionType::FarmTile, index, generation };
	return true;
}

void EditorSelectionSystem::SynchronizeFarmSelection(
	int index,
	uint64_t generation,
	int tileCount) noexcept {
	if (!SelectFarmTile(index, generation, tileCount)) {
		Clear();
	}
}

void EditorSelectionSystem::Clear() noexcept {
	selection_ = {};
}

const EditorSelection& EditorSelectionSystem::GetSelection() const noexcept {
	return selection_;
}

bool EditorSelectionSystem::IsFarmTileSelected(uint64_t generation) const noexcept {
	return selection_.type == EditorSelectionType::FarmTile &&
		selection_.generation == generation && selection_.index >= 0;
}

} // namespace editor
