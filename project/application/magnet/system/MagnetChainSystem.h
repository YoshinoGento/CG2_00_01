#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/BallMomentumTracker.h"
#include "application/magnet/system/BallReacquisitionCooldown.h"
#include "application/magnet/system/CircularArenaBoundary.h"
#include "application/magnet/system/MagneticImpactAttachmentSystem.h"
#include "application/magnet/system/ObstacleCollisionSystem.h"
#include "application/magnet/system/SpinChargeController.h"
#include "physics/PhysicsWorld.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace magnet {

// Owns runtime Player movement, loose-ball acquisition, chain topology, and release.
// Stage authoring/file IO and rendering remain outside this class.
class MagnetChainSystem final {
public:
	static constexpr std::size_t kLinksPerSide = 4;
	static constexpr std::size_t kStageBallCapacity = MagnetStageData::kMaximumBallCount;
	static constexpr float kAttachmentRadius = 2.15f;
	static constexpr float kMagnetDiameter = 1.0f;

	enum class GoalSize : uint8_t { Small, Standard, Large };

	struct Goal {
		Vector3 center{};
		float width = kMagnetDiameter * 2.5f;
		float depth = 1.5f;
		GoalSize size = GoalSize::Standard;
		std::size_t score = 1;
	};

	enum class StageBallState : uint8_t {
		Inactive,
		Available,
		AttachedLeft,
		AttachedRight,
		Released,
	};

	struct PlayerCommand {
		Vector3 moveDirection{};
		bool emergencyStop = false;
		bool releaseChains = false;
	};

	struct ReleaseConvergenceDiagnostics {
		Vector3 focusPoint{};
		float predictedRmsSpreadBefore = 0.0f;
		float predictedRmsSpreadAfter = 0.0f;
		float maximumDirectionCorrectionRadians = 0.0f;
		bool applied = false;
		bool valid = false;
	};

	[[nodiscard]] bool Initialize(const MagnetStageData& stageData);
	[[nodiscard]] bool Reset();
	[[nodiscard]] bool ApplyStageLayout(const MagnetStageData& stageData);
	void SetPlayerCommand(const PlayerCommand& command) noexcept { command_ = command; }
	void SetSpinChargeSettings(const SpinChargeController::Settings& settings) noexcept {
		spinChargeController_.SetSettings(settings);
	}
	void SetImpactAttachmentSettings(
		const MagneticImpactAttachmentSystem::Settings& settings) noexcept {
		impactAttachmentSystem_.SetSettings(settings);
	}
	void ConfigureGoal(GoalSize size, const Vector3& center) noexcept;
	[[nodiscard]] bool FixedUpdate(float fixedDeltaTime) noexcept;

	[[nodiscard]] const physics::PhysicsWorld& GetPhysicsWorld() const noexcept {
		return physicsWorld_;
	}
	[[nodiscard]] physics::BodyHandle GetPlayerBody() const noexcept { return playerBody_; }
	[[nodiscard]] const std::array<physics::BodyHandle, kLinksPerSide>&
		GetLeftChain() const noexcept { return leftChain_; }
	[[nodiscard]] const std::array<physics::BodyHandle, kLinksPerSide>&
		GetRightChain() const noexcept { return rightChain_; }
	[[nodiscard]] std::size_t GetLeftChainCount() const noexcept { return leftChainCount_; }
	[[nodiscard]] std::size_t GetRightChainCount() const noexcept { return rightChainCount_; }
	[[nodiscard]] std::size_t GetAttachedBallCount() const noexcept {
		return leftChainCount_ + rightChainCount_;
	}
	[[nodiscard]] const std::array<physics::BodyHandle, kStageBallCapacity>&
		GetStageBalls() const noexcept { return stageBalls_; }
	[[nodiscard]] const std::array<StageBallState, kStageBallCapacity>&
		GetStageBallStates() const noexcept { return stageBallStates_; }
	[[nodiscard]] const std::array<uint32_t, kStageBallCapacity>&
		GetStageBallIds() const noexcept { return stageBallIds_; }
	[[nodiscard]] std::size_t GetStageBallCount() const noexcept { return stageBallCount_; }
	[[nodiscard]] std::size_t GetAvailableBallCount() const noexcept;
	[[nodiscard]] std::size_t GetReleasedBallCount() const noexcept;
	[[nodiscard]] float GetMaximumConstraintError() const noexcept;
	[[nodiscard]] float GetPlayerHeadingRadians() const noexcept { return playerHeadingRadians_; }
	[[nodiscard]] static constexpr float GetAttachmentRadius() noexcept { return kAttachmentRadius; }
	[[nodiscard]] float GetArenaRadius() const noexcept { return arenaBoundary_.GetRadius(); }
	[[nodiscard]] bool HasAttachedBalls() const noexcept { return GetAttachedBallCount() > 0; }
	[[nodiscard]] const Goal& GetGoal() const noexcept { return goals_[0]; }
	[[nodiscard]] std::size_t GetGoalCount() const noexcept { return goalCount_; }
	[[nodiscard]] std::size_t GetGoalHitCount() const noexcept { return goalHitCount_; }
	[[nodiscard]] std::size_t GetScore() const noexcept { return score_; }
	[[nodiscard]] bool IsHealthy() const noexcept { return healthy_; }
	[[nodiscard]] float GetSpinChargeRatio() const noexcept { return spinChargeController_.GetChargeRatio(); }
	[[nodiscard]] float GetSpinChargeRotationRadians() const noexcept { return spinChargeController_.GetAccumulatedRotationRadians(); }
	[[nodiscard]] float GetSpinChargeSpeedMultiplier() const noexcept { return spinChargeController_.GetSpeedMultiplier(); }
	[[nodiscard]] float GetSpinChargeTurnSpeedMultiplier() const noexcept { return spinChargeController_.GetTurnSpeedMultiplier(); }
	[[nodiscard]] std::size_t GetMagneticAttachmentCount() const noexcept { return impactAttachmentSystem_.GetAttachmentCount(); }
	[[nodiscard]] const MagneticImpactAttachmentSystem::ImpactEvents&
	GetMagneticImpactEvents() const noexcept { return impactAttachmentSystem_.GetImpactEvents(); }
	[[nodiscard]] std::size_t GetMagneticImpactEventCount() const noexcept {
		return impactAttachmentSystem_.GetImpactEventCount();
	}
	[[nodiscard]] const ReleaseConvergenceDiagnostics&
		GetLastReleaseConvergenceDiagnostics() const noexcept {
		return lastReleaseConvergenceDiagnostics_;
	}

private:
	static constexpr std::size_t kBendConstraintsPerSide = kLinksPerSide - 1;
	static constexpr std::size_t kInvalidConstraintIndex =
		(std::numeric_limits<std::size_t>::max)();

	[[nodiscard]] bool RebuildRuntime();
	[[nodiscard]] bool CreateStageBallPool();
	[[nodiscard]] bool CreateConstraintSlots();
	[[nodiscard]] bool TryAttachNearestBall() noexcept;
	[[nodiscard]] bool AttachStageBall(std::size_t stageBallIndex, bool attachRight) noexcept;
	[[nodiscard]] Vector3 GetAttachmentTarget(bool rightSide) const noexcept;
	[[nodiscard]] bool ConfigureChainLink(
		bool rightSide,
		std::size_t linkIndex) noexcept;
	[[nodiscard]] bool ApplyMagneticRestoringForces(float fixedDeltaTime) noexcept;
	[[nodiscard]] bool UpdateMomentumTrackers(float fixedDeltaTime) noexcept;
	[[nodiscard]] bool ApplyMomentumLaunch() noexcept;
	[[nodiscard]] bool ReleaseChains() noexcept;
	[[nodiscard]] bool CollectReleasedMagnetsInGoal() noexcept;
	void DeactivateDistantReleasedBalls() noexcept;

	physics::PhysicsWorld physicsWorld_;
	physics::BodyHandle playerBody_{};
	std::array<physics::BodyHandle, kLinksPerSide> leftChain_{};
	std::array<physics::BodyHandle, kLinksPerSide> rightChain_{};
	std::array<std::size_t, kLinksPerSide> leftConstraintIndices_{};
	std::array<std::size_t, kLinksPerSide> rightConstraintIndices_{};
	std::array<std::size_t, kBendConstraintsPerSide> leftBendConstraintIndices_{};
	std::array<std::size_t, kBendConstraintsPerSide> rightBendConstraintIndices_{};
	BallMomentumTracker leftMomentumTracker_{};
	BallMomentumTracker rightMomentumTracker_{};
	BallReacquisitionCooldown reacquisitionCooldown_{};
	CircularArenaBoundary arenaBoundary_{};
	ObstacleCollisionSystem obstacleCollisionSystem_{};
	SpinChargeController spinChargeController_{};
	MagneticImpactAttachmentSystem impactAttachmentSystem_{};
	std::array<physics::BodyHandle, kStageBallCapacity> stageBalls_{};
	std::array<StageBallState, kStageBallCapacity> stageBallStates_{};
	std::array<uint32_t, kStageBallCapacity> stageBallIds_{};
	std::array<MagnetStageBallPlacement, kStageBallCapacity> stageLayoutBalls_{};
	std::array<MagnetStageBoxPlacement, MagnetStageData::kMaximumObstacleCount> obstacles_{};
	Vector3 stagePlayerPosition_{ 0.0f, 0.75f, 0.0f };
	PlayerCommand command_{};
	ReleaseConvergenceDiagnostics lastReleaseConvergenceDiagnostics_{};
	std::array<Goal, MagnetStageData::kMaximumGoalCount> goals_{};
	Vector3 playerVelocity_{};
	float playerHeadingRadians_ = 0.0f;
	std::size_t stageBallCount_ = 0;
	std::size_t obstacleCount_ = 0;
	std::size_t leftChainCount_ = 0;
	std::size_t rightChainCount_ = 0;
	std::size_t goalCount_ = 0;
	std::size_t goalHitCount_ = 0;
	std::size_t score_ = 0;
	bool healthy_ = false;
};

} // namespace magnet
