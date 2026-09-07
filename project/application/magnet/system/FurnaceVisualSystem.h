#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/MagnetChainSystem.h"
#include "2d/TextureManager.h"

#include <array>
#include <cstddef>
#include <memory>

class Camera;
class Model;
class ModelManager;
class Object3d;
class Object3dCommon;

namespace magnet {

// Owns Furnace-only render state and bounded visual proxies for dissolved balls.
// Gameplay removal remains authoritative in MagnetChainSystem.
class FurnaceVisualSystem final {
public:
	static constexpr std::size_t kMaximumDissolveEffects = 8;

	FurnaceVisualSystem() = default;
	~FurnaceVisualSystem();
	FurnaceVisualSystem(const FurnaceVisualSystem&) = delete;
	FurnaceVisualSystem& operator=(const FurnaceVisualSystem&) = delete;

	[[nodiscard]] bool Initialize(
		Object3dCommon* object3dCommon,
		ModelManager* modelManager,
		Camera* camera);
	void Finalize() noexcept;
	void Reset() noexcept;
	[[nodiscard]] bool AddDissolveEvents(
		const MagnetChainSystem::FurnaceDissolveEvents& events,
		std::size_t eventCount) noexcept;
	[[nodiscard]] bool Update(
		float deltaTime,
		const MagnetStageData& stageData,
		Camera* camera) noexcept;
	void Draw(const MagnetStageData& stageData) const;
	[[nodiscard]] bool IsReady() const noexcept { return ready_; }
	[[nodiscard]] std::size_t GetActiveEffectCount() const noexcept;

private:
	struct DissolveEffect {
		Vector3 position{};
		float radius = 0.0f;
		float ageSeconds = 0.0f;
		bool active = false;
	};

	Object3dCommon* object3dCommon_ = nullptr;
	std::unique_ptr<Model> furnaceModel_;
	std::unique_ptr<Model> dissolveBallModel_;
	std::array<std::unique_ptr<Object3d>, MagnetStageData::kMaximumObstacleCount>
		furnaceVisuals_{};
	std::array<std::unique_ptr<Object3d>, kMaximumDissolveEffects>
		dissolveVisuals_{};
	std::array<DissolveEffect, kMaximumDissolveEffects> dissolveEffects_{};
	Texture2DHandle magmaTexture_{};
	Texture2DHandle whiteTexture_{};
	Texture2DHandle noiseTexture_{};
	float elapsedSeconds_ = 0.0f;
	bool ready_ = false;
};

} // namespace magnet
