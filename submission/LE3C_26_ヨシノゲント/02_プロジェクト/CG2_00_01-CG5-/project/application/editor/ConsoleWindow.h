#pragma once

#include "base/Logger.h"
#include "editor/EditorLocalization.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

struct ConsoleWindowActions {
	bool clearRequested = false;
};

// Presents a cached Logger snapshot and emits editor actions only.
class ConsoleWindow final {
public:
	[[nodiscard]] ConsoleWindowActions Draw(EditorLanguage language);
	void SetOpen(bool open) noexcept { open_ = open; }
	[[nodiscard]] bool IsOpen() const noexcept { return open_; }

private:
	void RebuildVisibleEntries();
	[[nodiscard]] bool PassesFilter(const Logger::Entry& entry) const;

	std::vector<Logger::Entry> cachedEntries_;
	std::vector<std::size_t> visibleEntryIndices_;
	std::array<char, 256> searchBuffer_{};
	uint64_t loggerRevision_ = 0;
	uint64_t selectedSequence_ = 0;
	bool showInfo_ = true;
	bool showWarning_ = true;
	bool showError_ = true;
	bool autoScroll_ = true;
	bool open_ = true;
};
