#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Logger {
	enum class Level {
		Info,
		Warning,
		Error,
	};

	struct Entry {
		uint64_t sequence = 0;
		Level level = Level::Info;
		double secondsSinceStart = 0.0;
		uint32_t repeatCount = 1;
		std::string message;
	};

	void Log(const std::string& message);
	void Log(Level level, std::string_view message);
	void Info(std::string_view message);
	void Warning(std::string_view message);
	void Error(std::string_view message);
	void Clear();

	// Copies the ordered ring-buffer contents only when the revision changed.
	[[nodiscard]] bool CopyEntriesIfChanged(
		uint64_t& inOutRevision,
		std::vector<Entry>& output);
}

