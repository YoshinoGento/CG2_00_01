#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "effect/ComicTextEffect.h"
#include "physics/PhysicsWorld.h"

#include <array>

namespace magnet {
class MagnetChainSystem;

// Maps each stage-gimmick event to a compact comic sound-effect caption.
class GimmickComicTextSystem final {
public:
	[[nodiscard]] bool Initialize(ComicTextEffectSystem* effects);
	void Finalize() noexcept;
	void Reset() noexcept;
	void CaptureEvents(const MagnetChainSystem& chain, const MagnetStageData& stage);

private:
	enum class Preset : std::size_t {
		Chainsaw,
		Bumper,
		Furnace,
		Anchor,
		Shutter,
		Transfer,
		Repulsion,
		Count,
	};

	void Play(Preset preset, const Vector3& position);
	ComicTextEffectSystem* effects_ = nullptr;
	std::array<ComicTextEffectPreset, static_cast<std::size_t>(Preset::Count)> presets_{};
	std::array<physics::BodyHandle, MagnetStageData::kMaximumObstacleCount> anchored_{};
	std::array<bool, MagnetStageData::kMaximumObstacleCount> shutterClosed_{};
	std::array<bool, MagnetStageData::kMaximumObstacleCount> repulsionOccupied_{};
	bool snapshotReady_ = false;
};
} // namespace magnet
