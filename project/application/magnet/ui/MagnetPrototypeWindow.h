#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/MagnetChainSystem.h"

#include <array>
#include <cstddef>
#include <cstdint>

class SrvManager;

namespace magnet {

enum class MagnetEditorMode : uint8_t {
	Play,
	StageEdit,
};

struct MagnetPrototypeViewData {
	bool healthy = false;
	std::size_t bodyCount = 0;
	std::size_t constraintCount = 0;
	std::size_t activeConstraintCount = 0;
	std::size_t availableBallCount = 0;
	std::size_t attachedBallCount = 0;
	std::size_t releasedBallCount = 0;
	std::size_t leftChainCount = 0;
	std::size_t rightChainCount = 0;
	float playerSpeed = 0.0f;
	float maximumConstraintError = 0.0f;
	float attachmentRadius = 0.0f;
	float spinChargeRatio = 0.0f;
	float spinChargeRotations = 0.0f;
	float spinChargeSpeedMultiplier = 1.0f;
	float spinChargeTurnSpeedMultiplier = 1.0f;
	std::size_t magneticAttachmentCount = 0;
	std::size_t goalHitCount = 0;
	std::size_t score = 0;
	float goalWidth = 0.0f;
	const MagnetStageData* stageData = nullptr;
	const MagnetStageSaveEntry* saveEntries = nullptr;
	std::size_t saveEntryCount = 0;
	const char* stageOperationMessage = "";
	bool stageOperationSucceeded = false;
	bool stageDirty = false;
	MagnetEditorMode editorMode = MagnetEditorMode::Play;
};

enum class MagnetStageEditorAction : uint8_t {
	None,
	GenerateBalanced,
	SetArenaRadius,
	MovePlayer,
	AddBall,
	RemoveBall,
	MoveBall,
	AddGoal,
	AddObstacle,
	RemoveBoxObject,
	MoveBoxObject,
	UpdateGoalScore,
	UpdateObstacleKind,
	UpdateTransferPairId,
	SaveNamed,
	LoadNamed,
	RefreshSaves,
};

struct MagnetPrototypeUiRequest {
	MagnetStageEditorAction stageAction = MagnetStageEditorAction::None;
	MagnetStageGenerationSettings generationSettings{};
	float arenaRadius = 10.0f;
	std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1> stageSaveName{};
	uint32_t selectedBallId = 0;
	MagnetStageObjectType selectedObjectType = MagnetStageObjectType::None;
	uint32_t selectedObjectId = 0;
	Vector3 editedObjectPosition{};
	Vector3 editedObjectSize{ 1.0f, 1.0f, 1.0f };
	uint32_t editedGoalScore = 1;
	MagnetObstacleKind editedObstacleKind = MagnetObstacleKind::Solid;
	uint32_t editedTransferPairId = 1;
	bool allowOverwrite = false;
	SpinChargeController::Settings spinChargeSettings{};
	MagneticImpactAttachmentSystem::Settings impactAttachmentSettings{};
	bool modeChangeRequested = false;
	MagnetEditorMode requestedMode = MagnetEditorMode::Play;
	bool reset = false;
	bool emergencyStop = false;
	bool releaseChains = false;
	bool showGrid = true;
	bool showVelocity = true;
	bool cameraFollow = true;
};

// Presentation-only editor. It owns selection/widget state and emits typed requests.
class MagnetPrototypeWindow final {
public:
	MagnetPrototypeWindow();

	[[nodiscard]] MagnetPrototypeUiRequest Draw(
		const MagnetPrototypeViewData& viewData,
		SrvManager* srvManager,
		uint32_t finalDisplaySrvIndex,
		float virtualWidth,
		float virtualHeight);

private:
	void DrawMainMenuBar(
		const MagnetPrototypeViewData& viewData,
		MagnetPrototypeUiRequest& request);
	void BuildDefaultLayout(unsigned int dockspaceId);
	void DrawHierarchy(
		const MagnetPrototypeViewData& viewData,
		MagnetPrototypeUiRequest& request);
	void DrawViewport(
		const MagnetPrototypeViewData& viewData,
		SrvManager* srvManager,
		uint32_t finalDisplaySrvIndex,
		float virtualWidth,
		float virtualHeight);
	void DrawInspector(
		const MagnetPrototypeViewData& viewData,
		MagnetPrototypeUiRequest& request);
	void DrawStageEditor(
		const MagnetPrototypeViewData& viewData,
		MagnetPrototypeUiRequest& request);
	void DrawMonitor(const MagnetPrototypeViewData& viewData);
	void DrawStageSaveBrowser(
		const MagnetPrototypeViewData& viewData,
		MagnetPrototypeUiRequest& request);
	void CopySaveNameToRequest(
		MagnetPrototypeUiRequest& request,
		const std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1>& saveName,
		bool allowOverwrite) const noexcept;
	[[nodiscard]] bool SaveEntryExists(
		const MagnetPrototypeViewData& viewData,
		const char* saveName) const noexcept;

	MagnetStageGenerationSettings generationSettings_{};
	std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1> saveName_{};
	std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1> selectedSaveName_{};
	std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1> pendingSaveName_{};
	SpinChargeController::Settings spinChargeSettings_{};
	MagneticImpactAttachmentSystem::Settings impactAttachmentSettings_{};
	MagnetStageObjectType selectedObjectType_ = MagnetStageObjectType::None;
	uint32_t selectedObjectId_ = 0;
	MagnetObstacleKind obstaclePaletteKind_ = MagnetObstacleKind::Chainsaw;
	bool showGrid_ = true;
	bool showVelocity_ = true;
	bool cameraFollow_ = true;
	bool rebuildLayoutRequested_ = false;
};

} // namespace magnet
