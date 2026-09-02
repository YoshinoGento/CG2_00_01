#include "application/magnet/system/MagnetChainSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

constexpr float kPlayerRadius = 0.75f;
constexpr float kMagnetRadius = 0.5f;
constexpr float kFirstLinkLength = kPlayerRadius + kMagnetRadius;
constexpr float kFirstHorizontalOffset = 1.2247449f;
constexpr float kLinkLength = kMagnetRadius * 2.0f;
constexpr float kPlayerPlaneHeight = kPlayerRadius;
constexpr float kPlayerMaximumSpeed = 6.0f;
constexpr float kPlayerAcceleration = 12.0f;
constexpr float kPlayerDeceleration = 9.0f;
constexpr float kPlayerTurnRateRadians = 2.8f;
constexpr float kReleasedBallMaximumDistance = 32.0f;
constexpr float kDirectionEpsilonSquared = 1.0e-8f;
constexpr float kDynamicBodyMass = 1.0f;
constexpr float kDynamicBodyDamping = 0.12f;
constexpr float kConstraintCompliance = 0.00008f;
constexpr float kMaximumConstraintCorrection = 0.35f;
constexpr float kFirstBendHorizontalLength = kFirstHorizontalOffset + kLinkLength;
constexpr float kFirstBendHeightDifference = kPlayerRadius - kMagnetRadius;
const float kFirstBendRestLength = std::sqrt(
	kFirstBendHorizontalLength * kFirstBendHorizontalLength +
	kFirstBendHeightDifference * kFirstBendHeightDifference);
constexpr float kBendConstraintMaximumCorrection = 0.22f;
constexpr float kMaximumMagneticAcceleration = 65.0f;
constexpr float kMaximumMagneticVelocityChange = 1.40f;
constexpr float kMaximumLaunchSpeed = 22.0f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kDegreesToRadians = 0.01745329251994329577f;
constexpr float kReleaseConvergenceTimeSeconds = 0.45f;
constexpr float kReleaseConvergenceDirectionBlend = 0.25f;
constexpr float kReleaseConvergenceSpeedBlend = 0.10f;
constexpr float kReleaseConvergenceMaximumDirectionCorrectionRadians =
	12.0f * kDegreesToRadians;
constexpr float kReleaseConvergenceMaximumRelativeSpeedChange = 0.10f;
constexpr float kReleaseConvergenceMinimumSpeed = 0.20f;
constexpr float kReleaseConvergenceMinimumSpread = 0.05f;
constexpr std::array<float, MagnetChainSystem::kLinksPerSide> kSegmentStiffness = {
	58.0f, 38.0f, 26.0f, 18.0f,
};
constexpr std::array<float, MagnetChainSystem::kLinksPerSide> kSegmentDamping = {
	9.2f, 6.8f, 4.8f, 3.6f,
};
constexpr std::array<float, MagnetChainSystem::kLinksPerSide> kBendRetention = {
	0.06f, 0.30f, 0.55f, 0.70f,
};
constexpr std::array<float, MagnetChainSystem::kLinksPerSide> kMaximumBendRadians = {
	22.0f * kDegreesToRadians,
	30.0f * kDegreesToRadians,
	38.0f * kDegreesToRadians,
	48.0f * kDegreesToRadians,
};
constexpr std::array<float, MagnetChainSystem::kLinksPerSide - 1> kBendCompliance = {
	0.00022f, 0.00040f, 0.00065f,
};
constexpr uint32_t kPhysicsSubsteps = 3;
constexpr uint32_t kConstraintIterations = 8;
constexpr Vector3 kStandardGoalCenter = { 0.0f, 0.0f, 9.0f };
constexpr float kStandardGoalDepth = 1.5f;

bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float LengthSquaredXZ(const Vector3& value) noexcept
{
	return value.x * value.x + value.z * value.z;
}

float DotXZ(const Vector3& a, const Vector3& b) noexcept
{
	return a.x * b.x + a.z * b.z;
}

Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maximumDelta) noexcept
{
	const Vector3 delta = target - current;
	const float distanceSquared = LengthSquaredXZ(delta);
	if (distanceSquared <= maximumDelta * maximumDelta ||
		distanceSquared <= kDirectionEpsilonSquared) {
		return target;
	}
	return current + delta * (maximumDelta / std::sqrt(distanceSquared));
}

float MoveAngleTowards(float current, float target, float maximumDelta) noexcept
{
	const float delta = std::remainder(target - current, kTwoPi);
	if (std::abs(delta) <= maximumDelta) {
		return current + delta;
	}
	return current + std::copysign(maximumDelta, delta);
}

Vector3 ClampMagnitudeXZ(const Vector3& value, float maximumMagnitude) noexcept
{
	const float lengthSquared = LengthSquaredXZ(value);
	const float maximumMagnitudeSquared = maximumMagnitude * maximumMagnitude;
	if (lengthSquared <= maximumMagnitudeSquared ||
		lengthSquared <= kDirectionEpsilonSquared) {
		return value;
	}
	return value * (maximumMagnitude / std::sqrt(lengthSquared));
}

Vector3 NormalizeXZOr(const Vector3& value, const Vector3& fallback) noexcept
{
	const float lengthSquared = LengthSquaredXZ(value);
	if (!std::isfinite(lengthSquared) || lengthSquared <= kDirectionEpsilonSquared) {
		return fallback;
	}
	return value * (1.0f / std::sqrt(lengthSquared));
}

float SignedAngleXZ(const Vector3& from, const Vector3& to) noexcept
{
	const float crossY = from.z * to.x - from.x * to.z;
	return std::atan2(crossY, DotXZ(from, to));
}

bool SegmentIntersectsExpandedGoal(const Vector3& start, const Vector3& end,
	const MagnetChainSystem::Goal& goal, float radius) noexcept
{
	const float halfWidth = goal.width * 0.5f + radius;
	const float halfDepth = goal.depth * 0.5f + radius;
	float entryTime = 0.0f;
	float exitTime = 1.0f;
	const auto clipAxis = [&](float origin, float delta, float minimum, float maximum) noexcept {
		if (std::abs(delta) <= kDirectionEpsilonSquared) {
			return origin >= minimum && origin <= maximum;
		}
		float nearTime = (minimum - origin) / delta;
		float farTime = (maximum - origin) / delta;
		if (nearTime > farTime) { std::swap(nearTime, farTime); }
		entryTime = (std::max)(entryTime, nearTime);
		exitTime = (std::min)(exitTime, farTime);
		return entryTime <= exitTime;
	};
	return clipAxis(start.x, end.x - start.x,
		goal.center.x - halfWidth, goal.center.x + halfWidth) &&
		clipAxis(start.z, end.z - start.z,
		goal.center.z - halfDepth, goal.center.z + halfDepth);
}

Vector3 RotateXZ(const Vector3& value, float radians) noexcept
{
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	return {
		value.x * cosine + value.z * sine,
		0.0f,
		-value.x * sine + value.z * cosine,
	};
}

} // namespace

bool MagnetChainSystem::Initialize(const MagnetStageData& stageData)
{
	return ApplyStageLayout(stageData);
}

bool MagnetChainSystem::Reset()
{
	return RebuildRuntime();
}

void MagnetChainSystem::ConfigureGoal(GoalSize size, const Vector3& center) noexcept
{
	float widthInMagnets = 2.5f;
	if (size == GoalSize::Small) { widthInMagnets = 2.0f; }
	if (size == GoalSize::Large) { widthInMagnets = 4.5f; }
	goals_.fill({});
	goalCount_ = 1;
	goals_[0].center = IsFinite(center) ? center : kStandardGoalCenter;
	goals_[0].center.y = 0.0f;
	goals_[0].width = kMagnetDiameter * widthInMagnets;
	goals_[0].depth = kStandardGoalDepth;
	goals_[0].size = size;
}

bool MagnetChainSystem::ApplyStageLayout(const MagnetStageData& stageData)
{
	if (stageData.ballCount > stageLayoutBalls_.size() ||
		stageData.goalCount > stageData.goals.size() ||
		stageData.obstacleCount > obstacles_.size() ||
		!IsFinite(stageData.playerPosition)) {
		return false;
	}
	for (std::size_t index = 0; index < stageData.ballCount; ++index) {
		const MagnetStageBallPlacement& ball = stageData.balls[index];
		if (ball.id == 0 || !IsFinite(ball.position)) {
			return false;
		}
		for (std::size_t previousIndex = 0; previousIndex < index; ++previousIndex) {
			if (stageData.balls[previousIndex].id == ball.id) {
				return false;
			}
		}
	}

	stagePlayerPosition_ = stageData.playerPosition;
	stagePlayerPosition_.y = kPlayerPlaneHeight;
	stageBallCount_ = stageData.ballCount;
	stageLayoutBalls_.fill({});
	for (std::size_t index = 0; index < stageBallCount_; ++index) {
		stageLayoutBalls_[index] = stageData.balls[index];
		stageLayoutBalls_[index].position.y = kMagnetRadius;
	}
	obstacles_.fill({});
	obstacleCount_ = stageData.obstacleCount;
	for (std::size_t index = 0; index < obstacleCount_; ++index) {
		const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		if (obstacle.id == 0 || !IsFinite(obstacle.position) ||
			!IsFinite(obstacle.size) || obstacle.size.x <= 0.0f ||
			obstacle.size.y <= 0.0f || obstacle.size.z <= 0.0f) {
			return false;
		}
		obstacles_[index] = obstacle;
	}
	ConfigureGoal(GoalSize::Standard, kStandardGoalCenter);
	if (stageData.goalCount > 0) {
		goals_.fill({});
		goalCount_ = stageData.goalCount;
		for (std::size_t index = 0; index < goalCount_; ++index) {
			const MagnetStageBoxPlacement& authoredGoal = stageData.goals[index];
			if (authoredGoal.id == 0 || !IsFinite(authoredGoal.position) ||
				!IsFinite(authoredGoal.size) || authoredGoal.size.x <= 0.0f ||
				authoredGoal.size.z <= 0.0f) {
				return false;
			}
			goals_[index].center = authoredGoal.position;
			goals_[index].center.y = 0.0f;
			goals_[index].width = authoredGoal.size.x;
			goals_[index].depth = authoredGoal.size.z;
			goals_[index].size = GoalSize::Standard;
		}
	}
	return RebuildRuntime();
}

bool MagnetChainSystem::RebuildRuntime()
{
	physicsWorld_.Clear();
	playerVelocity_ = {};
	playerHeadingRadians_ = 0.0f;
	command_ = {};
	leftChain_.fill({});
	rightChain_.fill({});
	leftConstraintIndices_.fill(kInvalidConstraintIndex);
	rightConstraintIndices_.fill(kInvalidConstraintIndex);
	leftBendConstraintIndices_.fill(kInvalidConstraintIndex);
	rightBendConstraintIndices_.fill(kInvalidConstraintIndex);
	leftMomentumTracker_.Reset();
	rightMomentumTracker_.Reset();
	reacquisitionCooldown_.Reset();
	spinChargeController_.Reset();
	impactAttachmentSystem_.Reset();
	lastReleaseConvergenceDiagnostics_ = {};
	goalHitCount_ = 0;
	stageBalls_.fill({});
	stageBallStates_.fill(StageBallState::Inactive);
	stageBallIds_.fill(0);
	leftChainCount_ = 0;
	rightChainCount_ = 0;
	healthy_ = false;

	physics::SphereBodyDesc playerDesc{};
	playerDesc.position = stagePlayerPosition_;
	playerDesc.radius = kPlayerRadius;
	playerDesc.planeHeight = kPlayerPlaneHeight;
	playerDesc.motionType = physics::MotionType::Kinematic;
	playerBody_ = physicsWorld_.CreateSphereBody(playerDesc);
	if (!playerBody_.IsValid() || !CreateStageBallPool() || !CreateConstraintSlots()) {
		return false;
	}

	healthy_ = true;
	return true;
}

bool MagnetChainSystem::CreateStageBallPool()
{
	for (std::size_t index = 0; index < stageBalls_.size(); ++index) {
		physics::SphereBodyDesc bodyDesc{};
		bodyDesc.position = index < stageBallCount_
			? stageLayoutBalls_[index].position
			: Vector3{ 0.0f, kMagnetRadius, 0.0f };
		bodyDesc.radius = kMagnetRadius;
		bodyDesc.mass = kDynamicBodyMass;
		bodyDesc.linearDamping = kDynamicBodyDamping;
		bodyDesc.planeHeight = kMagnetRadius;
		bodyDesc.motionType = physics::MotionType::Dynamic;
		bodyDesc.active = index < stageBallCount_;
		stageBalls_[index] = physicsWorld_.CreateSphereBody(bodyDesc);
		if (!stageBalls_[index].IsValid()) {
			return false;
		}
		if (index < stageBallCount_) {
			stageBallStates_[index] = StageBallState::Available;
			stageBallIds_[index] = stageLayoutBalls_[index].id;
		}
	}
	return true;
}

bool MagnetChainSystem::CreateConstraintSlots()
{
	const physics::BodyHandle placeholder = stageBalls_.front();
	if (!placeholder.IsValid()) {
		return false;
	}
	const auto createSideSlots = [&](
		auto& constraintIndices,
		auto& bendConstraintIndices) {
		for (std::size_t linkIndex = 0; linkIndex < constraintIndices.size(); ++linkIndex) {
			physics::DistanceConstraintDesc desc{};
			desc.bodyA = playerBody_;
			desc.bodyB = placeholder;
			desc.restLength = linkIndex == 0 ? kFirstLinkLength : kLinkLength;
			desc.compliance = kConstraintCompliance;
			desc.maximumCorrection = kMaximumConstraintCorrection;
			desc.active = false;
			constraintIndices[linkIndex] = physicsWorld_.GetConstraintCount();
			if (!physicsWorld_.CreateDistanceConstraint(desc)) {
				return false;
			}
		}
		for (std::size_t bendIndex = 0;
			bendIndex < bendConstraintIndices.size();
			++bendIndex) {
			physics::DistanceConstraintDesc desc{};
			desc.bodyA = playerBody_;
			desc.bodyB = placeholder;
			desc.restLength = bendIndex == 0
				? kFirstBendRestLength
				: kLinkLength * 2.0f;
			desc.compliance = kBendCompliance[bendIndex];
			desc.maximumCorrection = kBendConstraintMaximumCorrection;
			desc.active = false;
			desc.debugDraw = false;
			bendConstraintIndices[bendIndex] = physicsWorld_.GetConstraintCount();
			if (!physicsWorld_.CreateDistanceConstraint(desc)) {
				return false;
			}
		}
		return true;
	};

	return createSideSlots(leftConstraintIndices_, leftBendConstraintIndices_) &&
		createSideSlots(rightConstraintIndices_, rightBendConstraintIndices_);
}

bool MagnetChainSystem::FixedUpdate(float fixedDeltaTime) noexcept
{
	if (!healthy_ || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f ||
		!IsFinite(command_.moveDirection)) {
		return false;
	}
	if (!reacquisitionCooldown_.Update(fixedDeltaTime)) {
		healthy_ = false;
		return false;
	}

	Vector3 requestedDirection = command_.moveDirection;
	requestedDirection.y = 0.0f;
	const float directionLengthSquared = LengthSquaredXZ(requestedDirection);
	Vector3 targetVelocity{};
	if (directionLengthSquared > kDirectionEpsilonSquared) {
		targetVelocity = requestedDirection *
			(kPlayerMaximumSpeed / std::sqrt(directionLengthSquared));
	}

	if (command_.emergencyStop) {
		playerVelocity_ = {};
	} else {
		const float acceleration = directionLengthSquared > kDirectionEpsilonSquared
			? kPlayerAcceleration
			: kPlayerDeceleration;
		playerVelocity_ = MoveTowards(
			playerVelocity_,
			targetVelocity,
			acceleration * fixedDeltaTime);
	}
	if (directionLengthSquared > kDirectionEpsilonSquared) {
		const float targetHeading = std::atan2(requestedDirection.x, requestedDirection.z);
		playerHeadingRadians_ = MoveAngleTowards(
			playerHeadingRadians_,
			targetHeading,
			kPlayerTurnRateRadians * spinChargeController_.GetTurnSpeedMultiplier() * fixedDeltaTime);
	}
	if (!spinChargeController_.Update(playerHeadingRadians_, fixedDeltaTime)) {
		healthy_ = false;
		return false;
	}

	if (!physicsWorld_.SetLinearVelocity(playerBody_, playerVelocity_) ||
		(!command_.releaseChains && !TryAttachNearestBall()) ||
		(HasAttachedBalls() && !ApplyMagneticRestoringForces(fixedDeltaTime)) ||
		(command_.releaseChains && HasAttachedBalls() && !ReleaseChains()) ||
		!physicsWorld_.Step(fixedDeltaTime, kPhysicsSubsteps, kConstraintIterations) ||
		(HasAttachedBalls() && !UpdateMomentumTrackers(fixedDeltaTime))) {
		healthy_ = false;
		return false;
	}
	// Goal collection uses the untouched swept path from the physics step.
	if (!CollectReleasedMagnetsInGoal()) {
		healthy_ = false;
		return false;
	}
	std::array<physics::BodyHandle, kStageBallCapacity + 1> arenaBodies{};
	arenaBodies[0] = playerBody_;
	for (std::size_t index = 0; index < stageBallCount_; ++index) {
		arenaBodies[index + 1] = stageBalls_[index];
	}
	if (!obstacleCollisionSystem_.Resolve(
		physicsWorld_, arenaBodies.data(), stageBallCount_ + 1,
		obstacles_.data(), obstacleCount_)) {
		healthy_ = false;
		return false;
	}
	if (!arenaBoundary_.Resolve(
		physicsWorld_, arenaBodies.data(), stageBallCount_ + 1)) {
		healthy_ = false;
		return false;
	}
	MagneticImpactAttachmentSystem::MagnetHandles releasedMagnets{};
	std::size_t releasedIndex = 0;
	for (std::size_t index = 0; index < stageBallCount_ && releasedIndex < releasedMagnets.size(); ++index) {
		if (stageBallStates_[index] == StageBallState::Released) {
			releasedMagnets[releasedIndex++] = stageBalls_[index];
		}
	}
	// Update every frame so one-shot impact events are cleared even after released
	// magnets become inactive or fewer than two remain.
	if (!impactAttachmentSystem_.Update(physicsWorld_, releasedMagnets, fixedDeltaTime)) {
		healthy_ = false;
		return false;
	}
	DeactivateDistantReleasedBalls();
	return true;
}

bool MagnetChainSystem::CollectReleasedMagnetsInGoal() noexcept
{
	for (std::size_t index = 0; index < stageBallCount_; ++index) {
		if (stageBallStates_[index] != StageBallState::Released) { continue; }
		const physics::BodyHandle handle = stageBalls_[index];
		const physics::SphereBody* body = physicsWorld_.GetBody(handle);
		if (!body || !body->active) { continue; }
		bool enteredGoal = false;
		for (std::size_t goalIndex = 0; goalIndex < goalCount_; ++goalIndex) {
			if (SegmentIntersectsExpandedGoal(
				body->previousPosition,
				body->position,
				goals_[goalIndex],
				body->radius)) {
				enteredGoal = true;
				break;
			}
		}
		if (!enteredGoal) { continue; }
		if (!physicsWorld_.SetLinearVelocity(handle, {}) ||
			!physicsWorld_.SetActive(handle, false)) { return false; }
		stageBallStates_[index] = StageBallState::Inactive;
		++goalHitCount_;
	}
	return true;
}

bool MagnetChainSystem::TryAttachNearestBall() noexcept
{
	if (leftChainCount_ >= kLinksPerSide && rightChainCount_ >= kLinksPerSide) {
		return true;
	}
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player || !player->active) {
		return false;
	}

	const bool leftAvailable = leftChainCount_ < kLinksPerSide;
	const bool rightAvailable = rightChainCount_ < kLinksPerSide;
	const Vector3 leftTarget = leftAvailable ? GetAttachmentTarget(false) : Vector3{};
	const Vector3 rightTarget = rightAvailable ? GetAttachmentTarget(true) : Vector3{};
	const float attachmentRadiusSquared = kAttachmentRadius * kAttachmentRadius;
	std::size_t bestStageBallIndex = stageBalls_.size();
	float bestCost = INFINITY;
	bool bestAttachRight = false;
	for (std::size_t index = 0; index < stageBallCount_; ++index) {
		if (stageBallStates_[index] != StageBallState::Available &&
			stageBallStates_[index] != StageBallState::Released) {
			continue;
		}
		if (stageBallStates_[index] == StageBallState::Released &&
			!reacquisitionCooldown_.CanReacquire(index)) {
			continue;
		}
		const physics::SphereBody* body = physicsWorld_.GetBody(stageBalls_[index]);
		if (!body || !body->active || !IsFinite(body->position)) {
			return false;
		}
		if (LengthSquaredXZ(body->position - player->position) > attachmentRadiusSquared) {
			continue;
		}

		const float leftCost = leftAvailable
			? LengthSquaredXZ(body->position - leftTarget)
			: INFINITY;
		const float rightCost = rightAvailable
			? LengthSquaredXZ(body->position - rightTarget)
			: INFINITY;
		const bool attachRight = rightCost < leftCost;
		const float cost = attachRight ? rightCost : leftCost;
		if (cost < bestCost) {
			bestCost = cost;
			bestStageBallIndex = index;
			bestAttachRight = attachRight;
		}
	}

	return bestStageBallIndex == stageBalls_.size() ||
		AttachStageBall(bestStageBallIndex, bestAttachRight);
}

bool MagnetChainSystem::AttachStageBall(
	std::size_t stageBallIndex,
	bool attachRight) noexcept
{
	if (stageBallIndex >= stageBallCount_ ||
		(stageBallStates_[stageBallIndex] != StageBallState::Available &&
		 stageBallStates_[stageBallIndex] != StageBallState::Released)) {
		return false;
	}
	const StageBallState previousState = stageBallStates_[stageBallIndex];
	if (previousState == StageBallState::Released &&
		(!impactAttachmentSystem_.DetachBody(physicsWorld_, stageBalls_[stageBallIndex]) ||
		 !physicsWorld_.SetLinearVelocity(stageBalls_[stageBallIndex], {}))) {
		return false;
	}
	auto& chain = attachRight ? rightChain_ : leftChain_;
	std::size_t& chainCount = attachRight ? rightChainCount_ : leftChainCount_;
	BallMomentumTracker& momentumTracker = attachRight
		? rightMomentumTracker_
		: leftMomentumTracker_;
	if (chainCount >= chain.size()) {
		return false;
	}

	const std::size_t linkIndex = chainCount;
	chain[linkIndex] = stageBalls_[stageBallIndex];
	stageBallStates_[stageBallIndex] = attachRight
		? StageBallState::AttachedRight
		: StageBallState::AttachedLeft;
	++chainCount;
	momentumTracker.Reset(linkIndex);
	if (!ConfigureChainLink(attachRight, linkIndex)) {
		--chainCount;
		chain[linkIndex] = {};
		stageBallStates_[stageBallIndex] = previousState;
		return false;
	}
	return true;
}

Vector3 MagnetChainSystem::GetAttachmentTarget(bool rightSide) const noexcept
{
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player) {
		return {};
	}
	const Vector3 sideAxis = {
		(rightSide ? 1.0f : -1.0f) * std::cos(playerHeadingRadians_),
		0.0f,
		(rightSide ? -1.0f : 1.0f) * std::sin(playerHeadingRadians_),
	};
	const auto& chain = rightSide ? rightChain_ : leftChain_;
	const std::size_t chainCount = rightSide ? rightChainCount_ : leftChainCount_;
	if (chainCount == 0) {
		return player->position + sideAxis * kFirstHorizontalOffset;
	}

	const physics::SphereBody* endpoint = physicsWorld_.GetBody(chain[chainCount - 1]);
	const physics::SphereBody* parent = chainCount == 1
		? player
		: physicsWorld_.GetBody(chain[chainCount - 2]);
	if (!endpoint || !parent) {
		return {};
	}
	const Vector3 outwardDirection = NormalizeXZOr(
		endpoint->position - parent->position,
		sideAxis);
	return endpoint->position + outwardDirection * kLinkLength;
}

bool MagnetChainSystem::ConfigureChainLink(
	bool rightSide,
	std::size_t linkIndex) noexcept
{
	auto& chain = rightSide ? rightChain_ : leftChain_;
	auto& constraintIndices = rightSide
		? rightConstraintIndices_
		: leftConstraintIndices_;
	auto& bendConstraintIndices = rightSide
		? rightBendConstraintIndices_
		: leftBendConstraintIndices_;
	if (linkIndex >= chain.size() || !chain[linkIndex].IsValid()) {
		return false;
	}

	physics::DistanceConstraintDesc neighborDesc{};
	neighborDesc.bodyA = linkIndex == 0 ? playerBody_ : chain[linkIndex - 1];
	neighborDesc.bodyB = chain[linkIndex];
	neighborDesc.restLength = linkIndex == 0 ? kFirstLinkLength : kLinkLength;
	neighborDesc.compliance = kConstraintCompliance;
	neighborDesc.maximumCorrection = kMaximumConstraintCorrection;
	neighborDesc.active = true;
	if (!physicsWorld_.ConfigureDistanceConstraint(
		constraintIndices[linkIndex],
		neighborDesc)) {
		return false;
	}

	if (linkIndex >= 1) {
		const std::size_t bendIndex = linkIndex - 1;
		physics::DistanceConstraintDesc bendDesc{};
		bendDesc.bodyA = linkIndex == 1 ? playerBody_ : chain[linkIndex - 2];
		bendDesc.bodyB = chain[linkIndex];
		bendDesc.restLength = bendIndex == 0
			? kFirstBendRestLength
			: kLinkLength * 2.0f;
		bendDesc.compliance = kBendCompliance[bendIndex];
		bendDesc.maximumCorrection = kBendConstraintMaximumCorrection;
		bendDesc.active = true;
		bendDesc.debugDraw = false;
		if (!physicsWorld_.ConfigureDistanceConstraint(
			bendConstraintIndices[bendIndex],
			bendDesc)) {
			(void)physicsWorld_.SetDistanceConstraintActive(
				constraintIndices[linkIndex],
				false);
			return false;
		}
	}
	return true;
}

bool MagnetChainSystem::ApplyMagneticRestoringForces(float fixedDeltaTime) noexcept
{
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player || !player->active) {
		return false;
	}
	const Vector3 rightAxis = {
		std::cos(playerHeadingRadians_),
		0.0f,
		-std::sin(playerHeadingRadians_),
	};
	const auto applyToChain = [&](
		float sideSign,
		const auto& chain,
		std::size_t chainCount) noexcept {
		Vector3 parentPosition = player->position;
		Vector3 parentVelocity = playerVelocity_;
		Vector3 parentDirection = rightAxis * sideSign;
		for (std::size_t linkIndex = 0; linkIndex < chainCount; ++linkIndex) {
			const physics::SphereBody* body = physicsWorld_.GetBody(chain[linkIndex]);
			if (!body || !body->active || body->motionType != physics::MotionType::Dynamic) {
				return false;
			}
			const Vector3 actualDirection = NormalizeXZOr(
				body->position - parentPosition,
				parentDirection);
			const float bendAngle = std::clamp(
				SignedAngleXZ(parentDirection, actualDirection),
				-kMaximumBendRadians[linkIndex],
				kMaximumBendRadians[linkIndex]);
			const Vector3 targetDirection = RotateXZ(
				parentDirection,
				bendAngle * kBendRetention[linkIndex]);
			const float segmentLength = linkIndex == 0
				? kFirstHorizontalOffset
				: kLinkLength;
			Vector3 targetPosition = parentPosition + targetDirection * segmentLength;
			targetPosition.y = body->planeHeight;
			Vector3 positionError = targetPosition - body->position;
			positionError.y = 0.0f;
			Vector3 relativeVelocity = body->linearVelocity - parentVelocity;
			relativeVelocity.y = 0.0f;
			Vector3 acceleration =
				positionError * kSegmentStiffness[linkIndex] -
				relativeVelocity * kSegmentDamping[linkIndex];
			acceleration = ClampMagnitudeXZ(acceleration, kMaximumMagneticAcceleration);
			const Vector3 correctedVelocity = body->linearVelocity + ClampMagnitudeXZ(
				acceleration * fixedDeltaTime,
				kMaximumMagneticVelocityChange);
			if (!IsFinite(correctedVelocity) ||
				!physicsWorld_.SetLinearVelocity(chain[linkIndex], correctedVelocity)) {
				return false;
			}
			parentPosition = body->position;
			parentVelocity = correctedVelocity;
			parentDirection = targetDirection;
		}
		return true;
	};

	return applyToChain(-1.0f, leftChain_, leftChainCount_) &&
		applyToChain(1.0f, rightChain_, rightChainCount_);
}

bool MagnetChainSystem::UpdateMomentumTrackers(float fixedDeltaTime) noexcept
{
	const auto updateChain = [&](
		const auto& chain,
		std::size_t chainCount,
		auto& momentumTracker) noexcept {
		for (std::size_t linkIndex = 0; linkIndex < chainCount; ++linkIndex) {
			const physics::SphereBody* body = physicsWorld_.GetBody(chain[linkIndex]);
			if (!body || !body->active ||
				!momentumTracker.Update(linkIndex, body->linearVelocity, fixedDeltaTime)) {
				return false;
			}
		}
		return true;
	};

	return updateChain(leftChain_, leftChainCount_, leftMomentumTracker_) &&
		updateChain(rightChain_, rightChainCount_, rightMomentumTracker_);
}

bool MagnetChainSystem::ApplyMomentumLaunch() noexcept
{
	constexpr std::size_t kMaximumReleasedBallCount = kLinksPerSide * 2;
	std::array<Vector3, kMaximumReleasedBallCount> releasePositions{};
	std::array<Vector3, kMaximumReleasedBallCount> releaseVelocities{};
	std::size_t releasedBallCount = 0;
	const auto gatherChain = [&](
		const auto& chain,
		std::size_t chainCount,
		const auto& momentumTracker) noexcept {
		for (std::size_t linkIndex = 0; linkIndex < chainCount; ++linkIndex) {
			const physics::SphereBody* body = physicsWorld_.GetBody(chain[linkIndex]);
			if (!body || !body->active || !IsFinite(body->position) ||
				!IsFinite(body->linearVelocity) ||
				releasedBallCount >= releasePositions.size()) {
				return false;
			}
			releasePositions[releasedBallCount] = body->position;
			releaseVelocities[releasedBallCount] =
				momentumTracker.CalculateLaunchVelocity(linkIndex, body->linearVelocity);
			releaseVelocities[releasedBallCount] =
				spinChargeController_.ApplyToLaunchVelocity(releaseVelocities[releasedBallCount]);
			if (!IsFinite(releaseVelocities[releasedBallCount])) {
				return false;
			}
			++releasedBallCount;
		}
		return true;
	};
	if (!gatherChain(leftChain_, leftChainCount_, leftMomentumTracker_) ||
		!gatherChain(rightChain_, rightChainCount_, rightMomentumTracker_) ||
		releasedBallCount == 0) {
		return false;
	}

	ReleaseConvergenceDiagnostics diagnostics{};
	Vector3 focusPoint{};
	for (std::size_t index = 0; index < releasedBallCount; ++index) {
		focusPoint += releasePositions[index] +
			releaseVelocities[index] * kReleaseConvergenceTimeSeconds;
	}
	const float inverseBallCount = 1.0f / static_cast<float>(releasedBallCount);
	focusPoint.x *= inverseBallCount;
	focusPoint.y *= inverseBallCount;
	focusPoint.z *= inverseBallCount;
	if (!IsFinite(focusPoint)) {
		return false;
	}
	diagnostics.focusPoint = focusPoint;

	const auto calculateRmsSpread = [&](const auto& velocities) noexcept {
		float squaredDistanceSum = 0.0f;
		for (std::size_t index = 0; index < releasedBallCount; ++index) {
			const Vector3 predictedPosition = releasePositions[index] +
				velocities[index] * kReleaseConvergenceTimeSeconds;
			const float squaredDistance = LengthSquaredXZ(predictedPosition - focusPoint);
			if (!std::isfinite(squaredDistance)) {
				return INFINITY;
			}
			squaredDistanceSum += squaredDistance;
		}
		return std::sqrt(squaredDistanceSum / static_cast<float>(releasedBallCount));
	};

	diagnostics.predictedRmsSpreadBefore = calculateRmsSpread(releaseVelocities);
	std::array<Vector3, kMaximumReleasedBallCount> convergenceVelocities =
		releaseVelocities;
	float maximumDirectionCorrection = 0.0f;
	if (releasedBallCount > 1 &&
		std::isfinite(diagnostics.predictedRmsSpreadBefore) &&
		diagnostics.predictedRmsSpreadBefore > kReleaseConvergenceMinimumSpread) {
		for (std::size_t index = 0; index < releasedBallCount; ++index) {
			const Vector3 rawVelocity = releaseVelocities[index];
			const float rawSpeedSquared = LengthSquaredXZ(rawVelocity);
			if (!std::isfinite(rawSpeedSquared) ||
				rawSpeedSquared <= kReleaseConvergenceMinimumSpeed *
					kReleaseConvergenceMinimumSpeed) {
				continue;
			}
			const float rawSpeed = std::sqrt(rawSpeedSquared);
			const Vector3 rawDirection = rawVelocity * (1.0f / rawSpeed);
			const Vector3 focusOffset = focusPoint - releasePositions[index];
			const float focusDistanceSquared = LengthSquaredXZ(focusOffset);
			if (!std::isfinite(focusDistanceSquared) ||
				focusDistanceSquared <= kDirectionEpsilonSquared) {
				continue;
			}
			const float focusDistance = std::sqrt(focusDistanceSquared);
			const Vector3 focusDirection = focusOffset * (1.0f / focusDistance);
			const float directionCorrection = std::clamp(
				SignedAngleXZ(rawDirection, focusDirection) *
					kReleaseConvergenceDirectionBlend,
				-kReleaseConvergenceMaximumDirectionCorrectionRadians,
				kReleaseConvergenceMaximumDirectionCorrectionRadians);
			const float desiredSpeed = focusDistance / kReleaseConvergenceTimeSeconds;
			const float correctedSpeed = std::clamp(
				rawSpeed + (desiredSpeed - rawSpeed) * kReleaseConvergenceSpeedBlend,
				rawSpeed * (1.0f - kReleaseConvergenceMaximumRelativeSpeedChange),
				rawSpeed * (1.0f + kReleaseConvergenceMaximumRelativeSpeedChange));
			convergenceVelocities[index] = ClampMagnitudeXZ(
				RotateXZ(rawDirection, directionCorrection) * correctedSpeed,
				kMaximumLaunchSpeed);
			maximumDirectionCorrection = (std::max)(
				maximumDirectionCorrection,
				std::abs(directionCorrection));
		}
	}

	diagnostics.predictedRmsSpreadAfter = calculateRmsSpread(convergenceVelocities);
	if (!std::isfinite(diagnostics.predictedRmsSpreadBefore) ||
		!std::isfinite(diagnostics.predictedRmsSpreadAfter) ||
		!std::isfinite(maximumDirectionCorrection)) {
		return false;
	}
	if (diagnostics.predictedRmsSpreadAfter < diagnostics.predictedRmsSpreadBefore) {
		releaseVelocities = convergenceVelocities;
		diagnostics.applied = true;
		diagnostics.maximumDirectionCorrectionRadians = maximumDirectionCorrection;
	} else {
		diagnostics.predictedRmsSpreadAfter = diagnostics.predictedRmsSpreadBefore;
	}
	diagnostics.valid = true;

	std::size_t velocityIndex = 0;
	const auto applyChain = [&](const auto& chain, std::size_t chainCount) noexcept {
		for (std::size_t linkIndex = 0; linkIndex < chainCount; ++linkIndex) {
			if (velocityIndex >= releasedBallCount ||
				!physicsWorld_.SetLinearVelocity(
					chain[linkIndex],
					releaseVelocities[velocityIndex])) {
				return false;
			}
			++velocityIndex;
		}
		return true;
	};
	if (!applyChain(leftChain_, leftChainCount_) ||
		!applyChain(rightChain_, rightChainCount_)) {
		return false;
	}
	lastReleaseConvergenceDiagnostics_ = diagnostics;
	spinChargeController_.ResetCharge();
	return true;
}

bool MagnetChainSystem::ReleaseChains() noexcept
{
	if (!HasAttachedBalls() || !ApplyMomentumLaunch()) {
		return false;
	}
	const auto deactivate = [&](const auto& indices) noexcept {
		for (std::size_t index : indices) {
			if (index == kInvalidConstraintIndex ||
				!physicsWorld_.SetDistanceConstraintActive(index, false)) {
				return false;
			}
		}
		return true;
	};
	if (!deactivate(leftConstraintIndices_) ||
		!deactivate(rightConstraintIndices_) ||
		!deactivate(leftBendConstraintIndices_) ||
		!deactivate(rightBendConstraintIndices_)) {
		return false;
	}

	for (std::size_t index = 0; index < stageBallCount_; ++index) {
		if (stageBallStates_[index] == StageBallState::AttachedLeft ||
			stageBallStates_[index] == StageBallState::AttachedRight) {
			stageBallStates_[index] = StageBallState::Released;
			reacquisitionCooldown_.Begin(index);
		}
	}
	leftChain_.fill({});
	rightChain_.fill({});
	leftChainCount_ = 0;
	rightChainCount_ = 0;
	leftMomentumTracker_.Reset();
	rightMomentumTracker_.Reset();
	impactAttachmentSystem_.BeginRelease();
	return true;
}

void MagnetChainSystem::DeactivateDistantReleasedBalls() noexcept
{
	const physics::SphereBody* player = physicsWorld_.GetBody(playerBody_);
	if (!player) {
		return;
	}
	const float maximumDistanceSquared =
		kReleasedBallMaximumDistance * kReleasedBallMaximumDistance;
	for (std::size_t index = 0; index < stageBallCount_; ++index) {
		if (stageBallStates_[index] != StageBallState::Released) {
			continue;
		}
		const physics::SphereBody* body = physicsWorld_.GetBody(stageBalls_[index]);
		if (!body || !body->active) {
			continue;
		}
		if (LengthSquaredXZ(body->position - player->position) > maximumDistanceSquared) {
			(void)physicsWorld_.SetActive(stageBalls_[index], false);
			stageBallStates_[index] = StageBallState::Inactive;
		}
	}
}

std::size_t MagnetChainSystem::GetAvailableBallCount() const noexcept
{
	return static_cast<std::size_t>(std::count(
		stageBallStates_.begin(),
		stageBallStates_.begin() + static_cast<std::ptrdiff_t>(stageBallCount_),
		StageBallState::Available));
}

std::size_t MagnetChainSystem::GetReleasedBallCount() const noexcept
{
	return static_cast<std::size_t>(std::count(
		stageBallStates_.begin(),
		stageBallStates_.begin() + static_cast<std::ptrdiff_t>(stageBallCount_),
		StageBallState::Released));
}

float MagnetChainSystem::GetMaximumConstraintError() const noexcept
{
	float maximumError = 0.0f;
	for (const physics::DistanceConstraint& constraint : physicsWorld_.GetConstraints()) {
		if (!constraint.active) {
			continue;
		}
		const physics::SphereBody* bodyA = physicsWorld_.GetBody(constraint.bodyA);
		const physics::SphereBody* bodyB = physicsWorld_.GetBody(constraint.bodyB);
		if (!bodyA || !bodyB || !bodyA->active || !bodyB->active) {
			continue;
		}
		const Vector3 delta = bodyB->position - bodyA->position;
		const float distance = std::sqrt(
			delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		if (!std::isfinite(distance)) {
			return INFINITY;
		}
		maximumError = (std::max)(
			maximumError,
			std::abs(distance - constraint.restLength));
	}
	return maximumError;
}

} // namespace magnet
