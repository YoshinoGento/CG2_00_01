#include "Logger.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <limits>
#include <mutex>
#include <string_view>

namespace {
constexpr std::size_t kLogCapacity = 1024;
constexpr std::size_t kMaximumMessageBytes = 16 * 1024;

struct LogStorage {
	std::array<Logger::Entry, kLogCapacity> entries;
	std::size_t first = 0;
	std::size_t size = 0;
	uint64_t nextSequence = 1;
	uint64_t revision = 1;
	std::mutex mutex;
	const std::chrono::steady_clock::time_point startTime =
		std::chrono::steady_clock::now();
};

LogStorage& GetStorage() {
	static LogStorage storage;
	return storage;
}

bool ContainsAsciiInsensitive(std::string_view text, std::string_view token) {
	return std::search(
		text.begin(), text.end(), token.begin(), token.end(),
		[](char left, char right) {
			const unsigned char leftByte = static_cast<unsigned char>(left);
			const unsigned char rightByte = static_cast<unsigned char>(right);
			return leftByte < 0x80u && rightByte < 0x80u
				? std::tolower(leftByte) == std::tolower(rightByte)
				: leftByte == rightByte;
		}) != text.end();
}

Logger::Level InferLevel(std::string_view message) {
	constexpr std::array errorTokens = {
		std::string_view("error"),
		std::string_view("failed"),
		std::string_view("failure"),
		std::string_view("invalid"),
		std::string_view("rejected"),
		std::string_view("could not"),
		std::string_view("missing"),
		std::string_view("exhausted"),
		std::string_view("unsupported"),
	};
	for (const std::string_view token : errorTokens) {
		if (ContainsAsciiInsensitive(message, token)) {
			return Logger::Level::Error;
		}
	}
	constexpr std::array warningTokens = {
		std::string_view("warning"),
		std::string_view("ignored"),
		std::string_view("fallback"),
		std::string_view("skipped"),
		std::string_view("continued without"),
		std::string_view("stale"),
	};
	for (const std::string_view token : warningTokens) {
		if (ContainsAsciiInsensitive(message, token)) {
			return Logger::Level::Warning;
		}
	}
	return Logger::Level::Info;
}
} // namespace

namespace Logger {
	void Log(const std::string& message) {
		Log(InferLevel(message), message);
	}

	void Log(Level level, std::string_view message) {
		std::string storedMessage(message.substr(0, kMaximumMessageBytes));
		OutputDebugStringA(storedMessage.c_str());
		OutputDebugStringA("\n");
		std::cout << storedMessage << std::endl;

		LogStorage& storage = GetStorage();
		{
			std::lock_guard lock(storage.mutex);
			if (storage.size > 0) {
				const std::size_t newestIndex =
					(storage.first + storage.size - 1) % kLogCapacity;
				Entry& newest = storage.entries[newestIndex];
				if (newest.level == level && newest.message == storedMessage) {
					if (newest.repeatCount < (std::numeric_limits<uint32_t>::max)()) {
						++newest.repeatCount;
					}
					++storage.revision;
					return;
				}
			}

			std::size_t destinationIndex = 0;
			if (storage.size < kLogCapacity) {
				destinationIndex = (storage.first + storage.size) % kLogCapacity;
				++storage.size;
			} else {
				destinationIndex = storage.first;
				storage.first = (storage.first + 1) % kLogCapacity;
			}
			Entry& destination = storage.entries[destinationIndex];
			destination.sequence = storage.nextSequence++;
			destination.level = level;
			destination.secondsSinceStart = std::chrono::duration<double>(
				std::chrono::steady_clock::now() - storage.startTime).count();
			destination.repeatCount = 1;
			destination.message = std::move(storedMessage);
			++storage.revision;
		}
	}

	void Info(std::string_view message) {
		Log(Level::Info, message);
	}

	void Warning(std::string_view message) {
		Log(Level::Warning, message);
	}

	void Error(std::string_view message) {
		Log(Level::Error, message);
	}

	void Clear() {
		LogStorage& storage = GetStorage();
		std::lock_guard lock(storage.mutex);
		for (std::size_t offset = 0; offset < storage.size; ++offset) {
			storage.entries[(storage.first + offset) % kLogCapacity] = {};
		}
		storage.first = 0;
		storage.size = 0;
		++storage.revision;
	}

	bool CopyEntriesIfChanged(
		uint64_t& inOutRevision,
		std::vector<Entry>& output) {
		LogStorage& storage = GetStorage();
		std::lock_guard lock(storage.mutex);
		if (inOutRevision == storage.revision) {
			return false;
		}
		output.clear();
		output.reserve(kLogCapacity);
		for (std::size_t offset = 0; offset < storage.size; ++offset) {
			output.push_back(storage.entries[(storage.first + offset) % kLogCapacity]);
		}
		inOutRevision = storage.revision;
		return true;
	}
}
