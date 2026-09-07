#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "2d/TextureManager.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

class Camera;
class Model;
class ModelManager;
class Object3d;
class Object3dCommon;

namespace magnet {

class MagnetChainSystem;
inline constexpr std::size_t kChainsawVisualFrameCount = 4;

// Owns render-only state for TransferGate, Chainsaw, RepulsionField, and MagneticAnchor.
// Gameplay state remains authoritative in MagnetChainSystem.
class MagnetGimmickVisualSystem final {
public:
	MagnetGimmickVisualSystem() = default;
	~MagnetGimmickVisualSystem();
	MagnetGimmickVisualSystem(const MagnetGimmickVisualSystem&) = delete;
	MagnetGimmickVisualSystem& operator=(const MagnetGimmickVisualSystem&) = delete;

	[[nodiscard]] bool Initialize(
		Object3dCommon* object3dCommon,
		ModelManager* modelManager,
		Camera* camera);
	void Finalize() noexcept;
	void Reset() noexcept;
	[[nodiscard]] bool Update(
		float deltaTime,
		const MagnetStageData& stageData,
		const MagnetChainSystem& chainSystem,
		Camera* camera) noexcept;
	void AddChainsawCutEffect(
		const MagnetStageData& stageData,
		const MagnetChainSystem& chainSystem) noexcept;
	void Draw(
		const MagnetStageData& stageData,
		const MagnetChainSystem& chainSystem) const;

	[[nodiscard]] bool IsReady() const noexcept { return ready_; }
	[[nodiscard]] static bool Supports(MagnetObstacleKind kind) noexcept;

private:
	struct VisualSlot {
		std::unique_ptr<Object3d> primary;
		std::unique_ptr<Object3d> secondary;
		MagnetObstacleKind configuredKind = MagnetObstacleKind::Count;
	};
	struct ChainsawSparkBurst {
		Vector3 position{};
		Vector3 outward{ 1.0f, 0.0f, 0.0f };
		float ageSeconds = 0.0f;
		uint32_t seed = 0;
		bool active = false;
	};
	static constexpr std::size_t kMaximumChainsawSparkBursts = 8;

	[[nodiscard]] bool ConfigureSlot(
		VisualSlot& slot,
		MagnetObstacleKind kind) noexcept;
	[[nodiscard]] bool UpdateTransferGate(
		VisualSlot& slot,
		const MagnetStageBoxPlacement& obstacle,
		Camera* camera,
		float deltaTime) noexcept;
	[[nodiscard]] bool UpdateChainsaw(
		VisualSlot& slot,
		const MagnetStageBoxPlacement& obstacle,
		Camera* camera,
		float deltaTime) noexcept;
	[[nodiscard]] bool UpdateMagneticAnchor(
		VisualSlot& slot,
		std::size_t obstacleIndex,
		const MagnetStageBoxPlacement& obstacle,
		const MagnetChainSystem& chainSystem,
		Camera* camera,
		float deltaTime) noexcept;
	[[nodiscard]] bool UpdateRepulsionField(
		VisualSlot& slot,
		const MagnetStageBoxPlacement& obstacle,
		const MagnetChainSystem& chainSystem,
		Camera* camera,
		float deltaTime) noexcept;

	Object3dCommon* object3dCommon_ = nullptr;
	Model* transferFrameModel_ = nullptr; // Borrowed from ModelManager.
	Model* chainsawBodyModel_ = nullptr; // Borrowed from ModelManager.
	std::array<Model*, kChainsawVisualFrameCount> chainsawChainModels_{}; // Borrowed.
	std::unique_ptr<Model> portalModel_;
	std::unique_ptr<Model> coreModel_;
	std::unique_ptr<Model> ringModel_;
	std::array<VisualSlot, MagnetStageData::kMaximumObstacleCount> slots_{};
	std::array<ChainsawSparkBurst, kMaximumChainsawSparkBursts> chainsawSparkBursts_{};
	Texture2DHandle whiteTexture_{};
	Texture2DHandle noiseTexture_{};
	float elapsedSeconds_ = 0.0f;
	std::size_t nextChainsawSparkBurst_ = 0;
	bool ready_ = false;
};

} // namespace magnet
