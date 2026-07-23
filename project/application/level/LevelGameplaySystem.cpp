#include "application/level/LevelGameplaySystem.h"

#include <algorithm>
#include <cmath>

namespace level {
namespace {

float SanitizeExtent(float value)
{
	return std::isfinite(value) ? (std::max)(std::abs(value), 0.05f) : 0.05f;
}

bool IsFiniteVector(const Vector3& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool OverlapsAabb(
	const Vector3& centerA,
	const Vector3& halfExtentsA,
	const Vector3& centerB,
	const Vector3& halfExtentsB)
{
	return
		std::abs(centerA.x - centerB.x) < halfExtentsA.x + halfExtentsB.x &&
		std::abs(centerA.y - centerB.y) < halfExtentsA.y + halfExtentsB.y &&
		std::abs(centerA.z - centerB.z) < halfExtentsA.z + halfExtentsB.z;
}

bool OverlapsAabbSphere(
	const Vector3& boxCenter,
	const Vector3& boxHalfExtents,
	const Vector3& sphereCenter,
	float sphereRadius)
{
	const Vector3 closestPoint = {
		std::clamp(sphereCenter.x, boxCenter.x - boxHalfExtents.x, boxCenter.x + boxHalfExtents.x),
		std::clamp(sphereCenter.y, boxCenter.y - boxHalfExtents.y, boxCenter.y + boxHalfExtents.y),
		std::clamp(sphereCenter.z, boxCenter.z - boxHalfExtents.z, boxCenter.z + boxHalfExtents.z),
	};
	const float dx = sphereCenter.x - closestPoint.x;
	const float dy = sphereCenter.y - closestPoint.y;
	const float dz = sphereCenter.z - closestPoint.z;
	return dx * dx + dy * dy + dz * dz <= sphereRadius * sphereRadius;
}

} // namespace

void LevelGameplaySystem::Reset()
{
	playerRenderIndex_ = 0;
	playerPosition_ = {};
	playerFacingDirection_ = { 0.0f, 0.0f, 1.0f };
	playerColliderCenterOffset_ = { 0.0f, 0.9f, 0.0f };
	playerColliderHalfExtents_ = { 0.45f, 0.9f, 0.45f };
	verticalVelocity_ = 0.0f;
	hasPlayer_ = false;
	isPlayerMoving_ = false;
	isPlayerSneaking_ = false;
	isPlayerGrounded_ = false;
	groundCenter_ = {};
	groundHalfExtents_ = {};
	hasGround_ = false;
	collectibles_.clear();
	obstacles_.clear();
	eventTriggers_.clear();
	collectedRenderIndices_.clear();
	triggeredEventIds_.clear();
	collectedCount_ = 0;
	stageCleared_ = false;
}

void LevelGameplaySystem::ConfigurePlayer(
	std::size_t renderIndex,
	const Vector3& position,
	const Vector3& colliderCenterOffset,
	const Vector3& colliderHalfExtents)
{
	playerRenderIndex_ = renderIndex;
	playerPosition_ = position;
	playerColliderCenterOffset_ = colliderCenterOffset;
	playerColliderHalfExtents_ = {
		SanitizeExtent(colliderHalfExtents.x),
		SanitizeExtent(colliderHalfExtents.y),
		SanitizeExtent(colliderHalfExtents.z),
	};
	hasPlayer_ = true;
	if (hasGround_) {
		playerPosition_.y = GetGroundSurfaceHeight();
		verticalVelocity_ = 0.0f;
		isPlayerGrounded_ = true;
	}
}

void LevelGameplaySystem::ConfigureGround(const Vector3& center, const Vector3& halfExtents)
{
	groundCenter_ = center;
	groundHalfExtents_ = {
		(std::max)(std::abs(halfExtents.x), 0.5f),
		(std::max)(std::abs(halfExtents.y), 0.0f),
		(std::max)(std::abs(halfExtents.z), 0.5f),
	};
	hasGround_ = true;
	if (hasPlayer_) {
		playerPosition_.y = GetGroundSurfaceHeight();
		verticalVelocity_ = 0.0f;
		isPlayerGrounded_ = true;
		SnapPlayerToGround();
	}
}

void LevelGameplaySystem::AddCollectible(std::size_t renderIndex, const Vector3& position, float radius)
{
	collectibles_.push_back({
		renderIndex,
		position,
		(std::max)(radius, 0.1f),
		false,
	});
}

void LevelGameplaySystem::AddObstacle(const Vector3& center, const Vector3& halfExtents)
{
	obstacles_.push_back({
		center,
		{
			(std::max)(std::abs(halfExtents.x), 0.05f),
			(std::max)(std::abs(halfExtents.y), 0.05f),
			(std::max)(std::abs(halfExtents.z), 0.05f),
		},
	});
}

bool LevelGameplaySystem::AddEventTrigger(
	const Vector3& center,
	const Vector3& halfExtents,
	int32_t eventId)
{
	if (eventId < 0 || !IsFiniteVector(center) || !IsFiniteVector(halfExtents)) {
		return false;
	}

	eventTriggers_.push_back({
		center,
		{
			SanitizeExtent(halfExtents.x),
			SanitizeExtent(halfExtents.y),
			SanitizeExtent(halfExtents.z),
		},
		eventId,
		false,
	});
	return true;
}

void LevelGameplaySystem::UpdatePlayer(const PlayerCommand& command, float fixedDeltaTime)
{
	if (!hasPlayer_ || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f) {
		return;
	}
	if (stageCleared_) {
		isPlayerMoving_ = false;
		isPlayerSneaking_ = false;
		return;
	}

	const Vector3& direction = command.moveDirection;
	const float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	isPlayerMoving_ = horizontalLength > 0.0001f;
	isPlayerSneaking_ = isPlayerMoving_ && command.sneakHeld;
	if (horizontalLength > 0.0001f) {
		playerFacingDirection_ = { direction.x / horizontalLength, 0.0f, direction.z / horizontalLength };
		const float moveSpeed = isPlayerSneaking_ ? sneakMoveSpeed_ : walkMoveSpeed_;
		const float step = moveSpeed * fixedDeltaTime / horizontalLength;
		MovePlayerHorizontal({ direction.x * step, 0.0f, direction.z * step });
	}

	if (command.jumpPressed && isPlayerGrounded_) {
		verticalVelocity_ = jumpSpeed_;
		isPlayerGrounded_ = false;
	}
	verticalVelocity_ -= gravity_ * fixedDeltaTime;
	playerPosition_.y += verticalVelocity_ * fixedDeltaTime;
	SnapPlayerToGround();
	CollectOverlappingObjects();
	ActivateOverlappingEventTriggers();
}

float LevelGameplaySystem::GetGroundSurfaceHeight() const
{
	return hasGround_ ? groundCenter_.y + groundHalfExtents_.y : playerPosition_.y;
}

std::vector<std::size_t> LevelGameplaySystem::ConsumeCollectedRenderIndices()
{
	std::vector<std::size_t> result = std::move(collectedRenderIndices_);
	collectedRenderIndices_.clear();
	return result;
}

std::vector<int32_t> LevelGameplaySystem::ConsumeTriggeredEventIds()
{
	std::vector<int32_t> result = std::move(triggeredEventIds_);
	triggeredEventIds_.clear();
	return result;
}

void LevelGameplaySystem::CaptureSnapshot(Snapshot& snapshot) const
{
	snapshot.playerPosition = playerPosition_;
	snapshot.playerFacingDirection = playerFacingDirection_;
	snapshot.verticalVelocity = verticalVelocity_;
	snapshot.isPlayerMoving = isPlayerMoving_;
	snapshot.isPlayerSneaking = isPlayerSneaking_;
	snapshot.isPlayerGrounded = isPlayerGrounded_;
	snapshot.collectibleStates.resize(collectibles_.size());
	for (std::size_t i = 0; i < collectibles_.size(); ++i) {
		snapshot.collectibleStates[i] = collectibles_[i].collected ? uint8_t{ 1 } : uint8_t{ 0 };
	}
	snapshot.eventTriggerStates.resize(eventTriggers_.size());
	for (std::size_t i = 0; i < eventTriggers_.size(); ++i) {
		snapshot.eventTriggerStates[i] = eventTriggers_[i].activated ? uint8_t{ 1 } : uint8_t{ 0 };
	}
	snapshot.collectedCount = collectedCount_;
	snapshot.stageCleared = stageCleared_;
}

bool LevelGameplaySystem::RestoreSnapshot(const Snapshot& snapshot)
{
	const std::size_t collectedStateCount = static_cast<std::size_t>(std::count_if(
		snapshot.collectibleStates.begin(), snapshot.collectibleStates.end(),
		[](uint8_t state) { return state != 0; }));
	const bool hasInvalidCollectibleState = std::any_of(
		snapshot.collectibleStates.begin(), snapshot.collectibleStates.end(),
		[](uint8_t state) { return state > 1; });
	const bool hasInvalidTriggerState = std::any_of(
		snapshot.eventTriggerStates.begin(), snapshot.eventTriggerStates.end(),
		[](uint8_t state) { return state > 1; });
	bool snapshotContainsStageClearTrigger = false;
	if (snapshot.eventTriggerStates.size() == eventTriggers_.size()) {
		for (std::size_t i = 0; i < eventTriggers_.size(); ++i) {
			if (snapshot.eventTriggerStates[i] != 0 && eventTriggers_[i].eventId == kStageClearEventId) {
				snapshotContainsStageClearTrigger = true;
				break;
			}
		}
	}
	if (!hasPlayer_ || snapshot.collectibleStates.size() != collectibles_.size() ||
		snapshot.eventTriggerStates.size() != eventTriggers_.size() ||
		snapshot.collectedCount > collectibles_.size() ||
		snapshot.collectedCount != collectedStateCount ||
		hasInvalidCollectibleState || hasInvalidTriggerState ||
		snapshot.stageCleared != snapshotContainsStageClearTrigger ||
		!IsFiniteVector(snapshot.playerPosition) ||
		!IsFiniteVector(snapshot.playerFacingDirection) ||
		!std::isfinite(snapshot.verticalVelocity)) {
		return false;
	}
	playerPosition_ = snapshot.playerPosition;
	playerFacingDirection_ = snapshot.playerFacingDirection;
	verticalVelocity_ = snapshot.verticalVelocity;
	isPlayerMoving_ = snapshot.isPlayerMoving;
	isPlayerSneaking_ = snapshot.isPlayerSneaking;
	isPlayerGrounded_ = snapshot.isPlayerGrounded;
	for (std::size_t i = 0; i < collectibles_.size(); ++i) {
		collectibles_[i].collected = snapshot.collectibleStates[i] != 0;
	}
	for (std::size_t i = 0; i < eventTriggers_.size(); ++i) {
		eventTriggers_[i].activated = snapshot.eventTriggerStates[i] != 0;
	}
	collectedCount_ = snapshot.collectedCount;
	stageCleared_ = snapshot.stageCleared;
	collectedRenderIndices_.clear();
	triggeredEventIds_.clear();
	return true;
}

void LevelGameplaySystem::SnapPlayerToGround()
{
	if (!hasPlayer_ || !hasGround_) {
		return;
	}

	const float minX = groundCenter_.x - groundHalfExtents_.x + playerColliderHalfExtents_.x - playerColliderCenterOffset_.x;
	const float maxX = groundCenter_.x + groundHalfExtents_.x - playerColliderHalfExtents_.x - playerColliderCenterOffset_.x;
	const float minZ = groundCenter_.z - groundHalfExtents_.z + playerColliderHalfExtents_.z - playerColliderCenterOffset_.z;
	const float maxZ = groundCenter_.z + groundHalfExtents_.z - playerColliderHalfExtents_.z - playerColliderCenterOffset_.z;
	playerPosition_.x = (minX <= maxX) ? std::clamp(playerPosition_.x, minX, maxX) : groundCenter_.x;
	playerPosition_.z = (minZ <= maxZ) ? std::clamp(playerPosition_.z, minZ, maxZ) : groundCenter_.z;
	const float groundHeight = GetGroundSurfaceHeight();
	if (playerPosition_.y <= groundHeight) {
		playerPosition_.y = groundHeight;
		verticalVelocity_ = 0.0f;
		isPlayerGrounded_ = true;
	} else {
		isPlayerGrounded_ = false;
	}
}

bool LevelGameplaySystem::OverlapsObstacle(const Vector3& position) const
{
	const Vector3 playerCenter = {
		position.x + playerColliderCenterOffset_.x,
		position.y + playerColliderCenterOffset_.y,
		position.z + playerColliderCenterOffset_.z,
	};
	for (const ObstacleCollider& obstacle : obstacles_) {
		if (OverlapsAabb(playerCenter, playerColliderHalfExtents_, obstacle.center, obstacle.halfExtents)) {
			return true;
		}
	}
	return false;
}

void LevelGameplaySystem::MovePlayerHorizontal(const Vector3& displacement)
{
	Vector3 candidate = playerPosition_;
	candidate.x += displacement.x;
	if (!OverlapsObstacle(candidate)) {
		playerPosition_.x = candidate.x;
	}

	candidate = playerPosition_;
	candidate.z += displacement.z;
	if (!OverlapsObstacle(candidate)) {
		playerPosition_.z = candidate.z;
	}
}

void LevelGameplaySystem::CollectOverlappingObjects()
{
	const Vector3 playerCenter = GetPlayerColliderCenter();
	for (CollectibleCollider& collectible : collectibles_) {
		if (collectible.collected) {
			continue;
		}

		if (!OverlapsAabbSphere(
				playerCenter,
				playerColliderHalfExtents_,
				collectible.position,
				collectible.radius)) {
			continue;
		}

		collectible.collected = true;
		collectedRenderIndices_.push_back(collectible.renderIndex);
		++collectedCount_;
	}
}

void LevelGameplaySystem::ActivateOverlappingEventTriggers()
{
	const Vector3 playerCenter = GetPlayerColliderCenter();
	for (EventTriggerCollider& trigger : eventTriggers_) {
		if (trigger.activated ||
			!OverlapsAabb(playerCenter, playerColliderHalfExtents_, trigger.center, trigger.halfExtents)) {
			continue;
		}

		trigger.activated = true;
		triggeredEventIds_.push_back(trigger.eventId);
		if (trigger.eventId == kStageClearEventId) {
			stageCleared_ = true;
		}
	}
}

} // namespace level
