#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/MagneticOneShotSoundSystem.h"
#include "physics/PhysicsWorld.h"

#include <array>

namespace magnet {
class MagnetChainSystem;

// Converts runtime gimmick events and state transitions into one-shot sounds.
class GimmickSoundSystem final {
public:
	[[nodiscard]] bool Initialize(Audio* audio);
	void Finalize();
	void Reset() noexcept;
	void Update(const MagnetChainSystem& chain, const MagnetStageData& stage);

private:
	MagneticOneShotSoundSystem bumper_;
	MagneticOneShotSoundSystem furnace_;
	MagneticOneShotSoundSystem anchor_;
	MagneticOneShotSoundSystem shutter_;
	MagneticOneShotSoundSystem transfer_;
	MagneticOneShotSoundSystem repulsion_;
	std::array<physics::BodyHandle, MagnetStageData::kMaximumObstacleCount> anchored_{};
	std::array<bool, MagnetStageData::kMaximumObstacleCount> shutterClosed_{};
	std::array<bool, MagnetStageData::kMaximumObstacleCount> repulsionOccupied_{};
	bool snapshotReady_ = false;
};
} // namespace magnet
