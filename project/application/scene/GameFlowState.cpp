#include "application/scene/GameFlowState.h"

#include "base/Logger.h"
#include "externals/nlohmann/json.hpp"

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>

namespace {
constexpr wchar_t kSaveDirectoryName[] = L"10DaysMagnet";
constexpr wchar_t kRankingFileName[] = L"ranking.json";
constexpr int kRankingSchemaVersion = 1;

[[nodiscard]] std::filesystem::path ResolveRankingPath() noexcept
{
	try {
		const DWORD requiredLength = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
		if (requiredLength <= 1) { return {}; }
		std::wstring localAppData(requiredLength, L'\0');
		const DWORD copiedLength = GetEnvironmentVariableW(
			L"LOCALAPPDATA", localAppData.data(), requiredLength);
		if (copiedLength == 0 || copiedLength >= requiredLength) { return {}; }
		localAppData.resize(copiedLength);
		return std::filesystem::path(localAppData) /
			kSaveDirectoryName / kRankingFileName;
	} catch (...) {
		return {};
	}
}

[[nodiscard]] bool ReadScore(
	const nlohmann::json& value, std::size_t& output) noexcept
{
	try {
		if (!value.is_number_integer() && !value.is_number_unsigned()) { return false; }
		if (value.is_number_integer() && value.get<std::int64_t>() < 0) { return false; }
		const std::uint64_t score = value.get<std::uint64_t>();
		if (score > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
			return false;
		}
		output = static_cast<std::size_t>(score);
		return true;
	} catch (...) {
		return false;
	}
}
} // namespace

GameFlowState::GameFlowState() noexcept
{
	LoadRanking();
}

void GameFlowState::SubmitScore(std::size_t score) noexcept
{
	if (rankingCount_ < ranking_.size()) {
		ranking_[rankingCount_++] = score;
	} else if (score > ranking_.back()) {
		ranking_.back() = score;
	} else {
		return;
	}
	std::sort(ranking_.begin(), ranking_.begin() + rankingCount_, std::greater<>());
	SaveRanking();
}

void GameFlowState::LoadRanking() noexcept
{
	ranking_.fill(0);
	rankingCount_ = 0;
	const std::filesystem::path path = ResolveRankingPath();
	if (path.empty()) {
		Logger::Log("GameFlowState: LOCALAPPDATA is unavailable; ranking will be session-only.");
		return;
	}

	std::error_code existsError;
	const bool fileExists = std::filesystem::is_regular_file(path, existsError);
	if (!fileExists || existsError) { return; }

	bool valid = false;
	try {
		std::ifstream file(path);
		nlohmann::json root;
		if (file.is_open()) { file >> root; }
		if (file.good() || file.eof()) {
			valid = root.is_object() && root.value("version", 0) == kRankingSchemaVersion &&
				root.contains("scores") && root["scores"].is_array() &&
				root["scores"].size() <= ranking_.size();
			if (valid) {
				for (const nlohmann::json& scoreJson : root["scores"]) {
					std::size_t score = 0;
					if (!ReadScore(scoreJson, score)) {
						valid = false;
						break;
					}
					ranking_[rankingCount_++] = score;
				}
			}
		}
	} catch (...) {
		valid = false;
	}

	if (valid) {
		std::sort(ranking_.begin(), ranking_.begin() + rankingCount_, std::greater<>());
		return;
	}

	ranking_.fill(0);
	rankingCount_ = 0;
	Logger::Log("GameFlowState: ranking save was invalid; reset to an empty ranking.");
	SaveRanking();
}

void GameFlowState::SaveRanking() const noexcept
{
	const std::filesystem::path path = ResolveRankingPath();
	if (path.empty()) { return; }
	try {
		std::error_code directoryError;
		std::filesystem::create_directories(path.parent_path(), directoryError);
		if (directoryError) {
			Logger::Log("GameFlowState: could not create the ranking save directory.");
			return;
		}

		nlohmann::json scores = nlohmann::json::array();
		for (std::size_t index = 0; index < rankingCount_; ++index) {
			scores.push_back(ranking_[index]);
		}
		const nlohmann::json root = {
			{ "version", kRankingSchemaVersion },
			{ "scores", std::move(scores) },
		};

		std::filesystem::path temporaryPath = path;
		temporaryPath += L".tmp";
		{
			std::ofstream file(temporaryPath, std::ios::trunc);
			if (!file.is_open()) {
				Logger::Log("GameFlowState: could not open the temporary ranking save.");
				return;
			}
			file << root.dump(4);
			if (!file.good()) {
				Logger::Log("GameFlowState: ranking save write failed.");
				return;
			}
		}

		if (!MoveFileExW(
			temporaryPath.c_str(), path.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			Logger::Log("GameFlowState: could not replace the ranking save file.");
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
		}
	} catch (...) {
		Logger::Log("GameFlowState: ranking save failed; gameplay will continue.");
	}
}
