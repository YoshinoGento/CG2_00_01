#pragma once

#include "math/Struct.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace level {

// Owns the gameplay state derived from level roles; rendering stays in GamePlayScene.
class LevelGameplaySystem {
public:
	struct PlayerCommand {
		Vector3 moveDirection{};
		bool jumpPressed = false;
		bool sneakHeld = false;
	};
	// Borrowed only for UpdatePlayer; neither context nor callback is retained.
	struct GroundHeightQuery {
		const void* context = nullptr;
		bool (*sample)(const void*, const Vector3&, float&) = nullptr;
	};
	struct Snapshot {
		Vector3 playerPosition{};
		Vector3 playerFacingDirection{};
		float verticalVelocity = 0.0f;
		bool isPlayerMoving = false;
		bool isPlayerSneaking = false;
		bool isPlayerGrounded = false;
		std::vector<uint8_t> collectibleStates;
		std::vector<uint8_t> eventTriggerStates;
		std::size_t collectedCount = 0;
		bool stageCleared = false;
	};

	struct CollectibleCollider {
		std::size_t renderIndex = 0;
		Vector3 position{};
		float radius = 1.0f;
		bool collected = false;
	};
	struct ObstacleCollider {
		Vector3 center{};
		Vector3 halfExtents{};
	};
	struct EventTriggerCollider {
		Vector3 center{};
		Vector3 halfExtents{};
		int32_t eventId = -1;
		bool activated = false;
	};

	static constexpr int32_t kStageClearEventId = 1;

	void Reset();
	void ConfigurePlayer(
		std::size_t renderIndex,
		const Vector3& position,
		const Vector3& colliderCenterOffset,
		const Vector3& colliderHalfExtents);
	void ConfigureGround(const Vector3& center, const Vector3& halfExtents);
	void AddCollectible(std::size_t renderIndex, const Vector3& position, float radius);
	void AddObstacle(const Vector3& center, const Vector3& halfExtents);
	bool AddEventTrigger(const Vector3& center, const Vector3& halfExtents, int32_t eventId);
	void UpdatePlayer(const PlayerCommand& command, float fixedDeltaTime, const GroundHeightQuery* groundQuery = nullptr);
	[[nodiscard]] bool TryPlacePlayerOnGround(const Vector3& position, const GroundHeightQuery* groundQuery = nullptr);
	void CaptureSnapshot(Snapshot& output) const;
	bool RestoreSnapshot(const Snapshot& snapshot);

	bool HasPlayer() const { return hasPlayer_; }
	std::size_t GetPlayerRenderIndex() const { return playerRenderIndex_; }
	const Vector3& GetPlayerPosition() const { return playerPosition_; }
	const Vector3& GetPlayerFacingDirection() const { return playerFacingDirection_; }
	bool IsPlayerMoving() const { return isPlayerMoving_; }
	bool IsPlayerSneaking() const { return isPlayerSneaking_; }
	bool IsPlayerGrounded() const { return isPlayerGrounded_; }
	bool WasPlayerMovementBlocked() const { return playerMovementBlocked_; }
	const Vector3& GetPlayerColliderHalfExtents() const { return playerColliderHalfExtents_; }
	const Vector3& GetPlayerColliderCenterOffset() const { return playerColliderCenterOffset_; }
	Vector3 GetPlayerColliderCenter() const {
		return {
			playerPosition_.x + playerColliderCenterOffset_.x,
			playerPosition_.y + playerColliderCenterOffset_.y,
			playerPosition_.z + playerColliderCenterOffset_.z,
		};
	}
	float GetGroundSurfaceHeight() const;
	bool HasGround() const { return hasGround_; }
	const Vector3& GetGroundCenter() const { return groundCenter_; }
	const Vector3& GetGroundHalfExtents() const { return groundHalfExtents_; }
	const std::vector<CollectibleCollider>& GetCollectibleColliders() const { return collectibles_; }
	const std::vector<ObstacleCollider>& GetObstacleColliders() const { return obstacles_; }
	const std::vector<EventTriggerCollider>& GetEventTriggerColliders() const { return eventTriggers_; }
	std::size_t GetCollectedCount() const { return collectedCount_; }
	std::size_t GetCollectibleCount() const { return collectibles_.size(); }
	bool IsStageCleared() const { return stageCleared_; }
	std::vector<std::size_t> ConsumeCollectedRenderIndices();
	std::vector<int32_t> ConsumeTriggeredEventIds();

private:
	void SnapPlayerToGround(const GroundHeightQuery* groundQuery = nullptr, bool followSlope = false);
	float SampleGroundHeight(const GroundHeightQuery* groundQuery, const Vector3& position) const;
	static constexpr float kMaxGroundStep = 0.25f;
	static constexpr float kSlopeFollowDistance = 0.12f;
	bool OverlapsObstacle(const Vector3& position) const;
	void MovePlayerHorizontal(const Vector3& displacement, const GroundHeightQuery* groundQuery, bool allowStep);
	static constexpr float kHorizontalSubstepDistance = 0.05f;
	static constexpr int kMaxHorizontalSubsteps = 32;
	void CollectOverlappingObjects();
	void ActivateOverlappingEventTriggers();

	std::size_t playerRenderIndex_ = 0;
	Vector3 playerPosition_{};
	Vector3 playerFacingDirection_ = { 0.0f, 0.0f, 1.0f };
	Vector3 playerColliderCenterOffset_ = { 0.0f, 0.9f, 0.0f };
	Vector3 playerColliderHalfExtents_ = { 0.45f, 0.9f, 0.45f };
	float verticalVelocity_ = 0.0f;
	bool hasPlayer_ = false;
	bool isPlayerMoving_ = false;
	bool isPlayerSneaking_ = false;
	bool isPlayerGrounded_ = false;
	bool playerMovementBlocked_ = false;

	Vector3 groundCenter_{};
	Vector3 groundHalfExtents_{};
	bool hasGround_ = false;

	float walkMoveSpeed_ = 2.25f;
	float sneakMoveSpeed_ = 1.25f;
	float jumpSpeed_ = 6.0f;
	float gravity_ = 18.0f;
	std::vector<CollectibleCollider> collectibles_;
	std::vector<ObstacleCollider> obstacles_;
	std::vector<EventTriggerCollider> eventTriggers_;
	std::vector<std::size_t> collectedRenderIndices_;
	std::vector<int32_t> triggeredEventIds_;
	std::size_t collectedCount_ = 0;
	bool stageCleared_ = false;
};

} // namespace level
