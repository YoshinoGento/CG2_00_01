#pragma once

#include "math/Struct.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace magnet {

struct MagnetStageBallPlacement {
	uint32_t id = 0;
	Vector3 position{};
};

struct MagnetStageGenerationSettings {
	uint32_t seed = 20260902u;
	std::size_t ballCount = 16;
	float minimumX = -9.0f;
	float maximumX = 9.0f;
	float minimumZ = -9.0f;
	float maximumZ = 9.0f;
	float minimumSpacing = 2.0f;
	float playerClearRadius = 3.0f;
};

struct MagnetStageData {
	static constexpr uint32_t kSchemaVersion = 1;
	static constexpr std::size_t kMaximumBallCount = 24;

	std::string name = "stage_01";
	MagnetStageGenerationSettings generation{};
	std::array<MagnetStageBallPlacement, kMaximumBallCount> balls{};
	std::size_t ballCount = 0;
};

struct MagnetStageSaveEntry {
	std::string name;
	std::string relativePath;
};

// Owns editable magnet-stage data and explicit JSON/generation operations.
// Runtime PhysicsWorld state remains owned by MagnetChainSystem.
class MagnetStageSystem final {
public:
	static constexpr std::size_t kMaximumSaveEntryCount = 64;
	static constexpr std::size_t kMaximumSaveNameLength = 48;
	static constexpr const char* kDefaultStageDirectory =
		"Resources/levels/magnet";
	static constexpr const char* kDefaultStagePath =
		"Resources/levels/magnet/stage_01.json";

	explicit MagnetStageSystem(
		std::string saveDirectory = kDefaultStageDirectory);

	[[nodiscard]] bool Initialize();
	[[nodiscard]] bool GenerateBalanced(
		const MagnetStageGenerationSettings& settings);
	[[nodiscard]] bool AddBall(const Vector3& position);
	[[nodiscard]] bool RemoveBall(uint32_t id);
	[[nodiscard]] bool SetBallPosition(uint32_t id, const Vector3& position);
	[[nodiscard]] bool Save(const std::string& path);
	[[nodiscard]] bool Load(const std::string& path);
	[[nodiscard]] bool RefreshSaveEntries();
	[[nodiscard]] bool SaveNamed(const std::string& saveName, bool allowOverwrite);
	[[nodiscard]] bool LoadNamed(const std::string& saveName);

	[[nodiscard]] const MagnetStageData& GetStageData() const noexcept { return stageData_; }
	[[nodiscard]] const std::array<MagnetStageSaveEntry, kMaximumSaveEntryCount>&
		GetSaveEntries() const noexcept { return saveEntries_; }
	[[nodiscard]] std::size_t GetSaveEntryCount() const noexcept { return saveEntryCount_; }
	[[nodiscard]] bool IsDirty() const noexcept { return dirty_; }
	[[nodiscard]] const MagnetStageBallPlacement* FindBall(uint32_t id) const noexcept;
	[[nodiscard]] const std::string& GetLastOperationMessage() const noexcept {
		return lastOperationMessage_;
	}
	[[nodiscard]] bool DidLastOperationSucceed() const noexcept {
		return lastOperationSucceeded_;
	}

private:
	[[nodiscard]] static bool IsFinitePosition(const Vector3& position) noexcept;
	[[nodiscard]] static bool IsSafeJsonPath(const std::string& path);
	[[nodiscard]] static bool IsSafeSaveName(const std::string& saveName) noexcept;
	[[nodiscard]] static bool ValidateGenerationSettings(
		const MagnetStageGenerationSettings& settings) noexcept;
	[[nodiscard]] static bool ValidateStageData(const MagnetStageData& stageData) noexcept;
	[[nodiscard]] bool BuildSavePath(
		const std::string& saveName,
		std::string& outputPath) const;
	[[nodiscard]] bool RefreshSaveEntriesInternal() noexcept;
	void SetOperationResult(bool succeeded, const std::string& message);

	std::string saveDirectory_;
	MagnetStageData stageData_{};
	std::array<MagnetStageSaveEntry, kMaximumSaveEntryCount> saveEntries_{};
	std::string lastOperationMessage_ = "Stage data is not initialized.";
	std::size_t saveEntryCount_ = 0;
	bool lastOperationSucceeded_ = false;
	bool dirty_ = false;
};

} // namespace magnet
