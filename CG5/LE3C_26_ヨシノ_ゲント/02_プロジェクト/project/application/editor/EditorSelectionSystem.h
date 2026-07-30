#pragma once

#include <cstdint>

namespace editor {

enum class EditorSelectionType {
	None,
	FarmTile,
};

struct EditorSelection {
	EditorSelectionType type = EditorSelectionType::None;
	int index = -1;
	uint64_t generation = 0;
};

class EditorSelectionSystem final {
public:
	bool SelectFarmTile(int index, uint64_t generation, int tileCount) noexcept;
	void SynchronizeFarmSelection(int index, uint64_t generation, int tileCount) noexcept;
	void Clear() noexcept;

	[[nodiscard]] const EditorSelection& GetSelection() const noexcept;
	[[nodiscard]] bool IsFarmTileSelected(uint64_t generation) const noexcept;

private:
	EditorSelection selection_{};
};

} // namespace editor
