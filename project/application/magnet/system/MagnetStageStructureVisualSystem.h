#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"

#include <array>
#include <cstddef>
#include <memory>

class Camera;
class Model;
class ModelManager;
class Object3d;
class Object3dCommon;

namespace magnet {

class MagnetChainSystem;

// Owns render-only Goal and Solid Wall presentation.
// Stage collision and scoring remain authoritative in gameplay Systems.
class MagnetStageStructureVisualSystem final {
public:
	MagnetStageStructureVisualSystem() = default;
	~MagnetStageStructureVisualSystem();
	MagnetStageStructureVisualSystem(const MagnetStageStructureVisualSystem&) = delete;
	MagnetStageStructureVisualSystem& operator=(
		const MagnetStageStructureVisualSystem&) = delete;

	[[nodiscard]] bool Initialize(
		Object3dCommon* object3dCommon,
		ModelManager* modelManager,
		Camera* camera);
	void Finalize() noexcept;
	void Reset() noexcept;
	[[nodiscard]] bool Update(
		float deltaTime,
		const MagnetStageData& stageData,
		Camera* camera,
		const MagnetChainSystem* chainSystem = nullptr) noexcept;
	void Draw(const MagnetStageData& stageData) const;

	[[nodiscard]] bool IsReady() const noexcept { return ready_; }
	[[nodiscard]] static bool SupportsObstacle(MagnetObstacleKind kind) noexcept;

private:
	void DrawGoalSuction(const MagnetStageBoxPlacement& goal) const noexcept;

	Object3dCommon* object3dCommon_ = nullptr;
	Model* goalModel_ = nullptr; // Borrowed from ModelManager.
	Model* wallModel_ = nullptr; // Borrowed from ModelManager.
	std::array<std::unique_ptr<Object3d>, MagnetStageData::kMaximumGoalCount>
		goalVisuals_{};
	std::array<std::unique_ptr<Object3d>, MagnetStageData::kMaximumObstacleCount>
		wallVisuals_{};
	std::array<MagnetStageBoxPlacement, MagnetStageData::kMaximumGoalCount>
		runtimeGoals_{};
	float elapsedSeconds_ = 0.0f;
	bool ready_ = false;
};

} // namespace magnet
