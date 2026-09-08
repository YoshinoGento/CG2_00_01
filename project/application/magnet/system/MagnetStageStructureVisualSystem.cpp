#include "application/magnet/system/MagnetStageStructureVisualSystem.h"
#include "application/magnet/system/MagnetChainSystem.h"

#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "base/FrameClock.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>

namespace magnet {
namespace {

constexpr char kGoalModelPath[] = "magnet/goal/goal.obj";
constexpr char kGoalFilePath[] = "Resources/magnet/goal/goal.obj";
constexpr char kWallModelPath[] = "magnet/wall/wall.obj";
constexpr char kWallFilePath[] = "Resources/magnet/wall/wall.obj";
constexpr float kHalfPi = std::numbers::pi_v<float> * 0.5f;
constexpr float kTau = std::numbers::pi_v<float> * 2.0f;
constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
constexpr int kGoalLightningBranchCount = 7;
constexpr int kGoalLightningSegmentCount = 7;
constexpr Vector4 kGoalLightningColor{ 0.04f, 0.62f, 1.0f, 0.78f };
constexpr Vector4 kGoalLightningBrightColor{ 0.78f, 0.96f, 1.0f, 1.0f };

[[nodiscard]] bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

[[nodiscard]] bool IsPositiveFiniteSize(const Vector3& value) noexcept
{
	return IsFinite(value) && value.x > 0.0f && value.y > 0.0f && value.z > 0.0f;
}

[[nodiscard]] bool IsRegularFile(const char* path) noexcept
{
	std::error_code error;
	return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] float WrappedDistance(float left, float right) noexcept
{
	const float distance = std::abs(left - right);
	return (std::min)(distance, 1.0f - distance);
}

[[nodiscard]] Vector3 RotateXZ(const Vector3& value, float radians) noexcept
{
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	return {
		value.x * cosine + value.z * sine,
		value.y,
		-value.x * sine + value.z * cosine,
	};
}

} // namespace

MagnetStageStructureVisualSystem::~MagnetStageStructureVisualSystem() = default;

bool MagnetStageStructureVisualSystem::Initialize(
	Object3dCommon* object3dCommon,
	ModelManager* modelManager,
	Camera* camera)
{
	Finalize();
	if (!object3dCommon || !modelManager || !camera ||
		!IsRegularFile(kGoalFilePath) || !IsRegularFile(kWallFilePath)) {
		return false;
	}

	object3dCommon_ = object3dCommon;
	modelManager->LoadModel(kGoalModelPath);
	modelManager->LoadModel(kWallModelPath);
	goalModel_ = modelManager->GetModel(kGoalModelPath);
	wallModel_ = modelManager->GetModel(kWallModelPath);
	if (!goalModel_ || !wallModel_) {
		Finalize();
		return false;
	}
	goalModel_->LoadTextures();
	wallModel_->LoadTextures();

	const auto createVisual = [this](Model* model) {
		auto visual = std::make_unique<Object3d>();
		visual->Initialize(object3dCommon_);
		visual->SetModel(model);
		visual->SetEnableLighting(true);
		visual->SetShininess(72.0f);
		visual->SetCullMode(2);
		return visual;
	};
	for (auto& visual : goalVisuals_) {
		visual = createVisual(goalModel_);
	}
	for (auto& visual : wallVisuals_) {
		visual = createVisual(wallModel_);
	}

	Reset();
	ready_ = Update(0.0f, {}, camera);
	return ready_;
}

void MagnetStageStructureVisualSystem::Finalize() noexcept
{
	ready_ = false;
	Reset();
	for (auto& visual : wallVisuals_) { visual.reset(); }
	for (auto& visual : goalVisuals_) { visual.reset(); }
	wallModel_ = nullptr;
	goalModel_ = nullptr;
	object3dCommon_ = nullptr;
}

void MagnetStageStructureVisualSystem::Reset() noexcept
{
	elapsedSeconds_ = 0.0f;
}

bool MagnetStageStructureVisualSystem::SupportsObstacle(
	MagnetObstacleKind kind) noexcept
{
	return kind == MagnetObstacleKind::Solid;
}

bool MagnetStageStructureVisualSystem::Update(
	float deltaTime,
	const MagnetStageData& stageData,
	Camera* camera,
	const MagnetChainSystem* chainSystem) noexcept
{
	if (!object3dCommon_ || !goalModel_ || !wallModel_ || !camera ||
		stageData.goalCount > goalVisuals_.size() ||
		stageData.obstacleCount > wallVisuals_.size() ||
		!std::isfinite(deltaTime) || deltaTime < 0.0f) {
		return false;
	}

	deltaTime = (std::min)(deltaTime, FrameClock::kMaximumFrameDeltaSeconds);
	elapsedSeconds_ = std::remainder(elapsedSeconds_ + deltaTime, 4096.0f);
	for (std::size_t index = 0; index < stageData.goalCount; ++index) {
		runtimeGoals_[index] = stageData.goals[index];
		if (chainSystem && index < chainSystem->GetGoalCount()) {
			runtimeGoals_[index].position = chainSystem->GetGoal(index).center;
			runtimeGoals_[index].position.y = stageData.goals[index].position.y;
		}
		const MagnetStageBoxPlacement& goal = runtimeGoals_[index];
		if (!IsFinite(goal.position) || !IsPositiveFiniteSize(goal.size) ||
			!std::isfinite(goal.rotationYDegrees) ||
			!goalVisuals_[index]) {
			return false;
		}
		const bool normalAlongX = goal.size.x <= goal.size.z;
		Object3d& visual = *goalVisuals_[index];
		visual.SetPosition(goal.position);
		visual.SetRotation({
			0.0f,
			(normalAlongX ? kHalfPi : 0.0f) +
				goal.rotationYDegrees * kDegreesToRadians,
			0.0f });
		visual.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		const Vector3 scale = normalAlongX
			? Vector3{ goal.size.z, goal.size.y, goal.size.x }
			: goal.size;
		if (!visual.SetScale(scale) ||
			!visual.SetSurfaceTextureTransform({ 1.0f, 1.0f }, {})) {
			return false;
		}
		visual.Update(camera, deltaTime);
	}

	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		const MagnetStageBoxPlacement& wall = stageData.obstacles[index];
		if (!SupportsObstacle(wall.obstacleKind)) {
			continue;
		}
		if (!IsFinite(wall.position) || !IsPositiveFiniteSize(wall.size) ||
			!std::isfinite(wall.rotationYDegrees) ||
			!wallVisuals_[index]) {
			return false;
		}
		const bool normalAlongX = wall.size.x <= wall.size.z;
		Object3d& visual = *wallVisuals_[index];
		visual.SetPosition(wall.position);
		visual.SetRotation({
			0.0f,
			(normalAlongX ? kHalfPi : 0.0f) +
				wall.rotationYDegrees * kDegreesToRadians,
			0.0f });
		visual.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		const Vector3 scale = normalAlongX
			? Vector3{ wall.size.z, wall.size.y, wall.size.x }
			: wall.size;
		if (!visual.SetScale(scale) ||
			!visual.SetSurfaceTextureTransform({ 1.0f, 1.0f }, {})) {
			return false;
		}
		visual.Update(camera, deltaTime);
	}
	return true;
}

void MagnetStageStructureVisualSystem::DrawGoalSuction(
	const MagnetStageBoxPlacement& goal) const noexcept
{
	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	if (!lineDrawer || !IsFinite(goal.position) || !IsPositiveFiniteSize(goal.size) ||
		!std::isfinite(goal.rotationYDegrees)) {
		return;
	}

	const bool normalAlongX = goal.size.x <= goal.size.z;
	const float openingWidth = (normalAlongX ? goal.size.z : goal.size.x) * 0.62f;
	const float openingHeight = goal.size.y * 0.62f;
	const float depth = normalAlongX ? goal.size.x : goal.size.z;
	const Vector3 localHorizontal = normalAlongX
		? Vector3{ 0.0f, 0.0f, 1.0f }
		: Vector3{ 1.0f, 0.0f, 0.0f };
	const Vector3 localNormal = normalAlongX
		? Vector3{ -1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 0.0f, -1.0f };
	const float rotationYRadians = goal.rotationYDegrees * kDegreesToRadians;
	const Vector3 horizontal = RotateXZ(localHorizontal, rotationYRadians);
	const Vector3 normal = RotateXZ(localNormal, rotationYRadians);
	const Vector3 center = goal.position + normal * (depth * 0.51f + 0.025f);

	for (int branch = 0; branch < kGoalLightningBranchCount; ++branch) {
		const float branchRatio = static_cast<float>(branch) /
			static_cast<float>(kGoalLightningBranchCount);
		const float baseAngle = branchRatio * kTau +
			elapsedSeconds_ * 0.72f + static_cast<float>(goal.id) * 0.19f;
		const float brightPosition = std::fmod(
			elapsedSeconds_ * 1.85f + branchRatio, 1.0f);
		Vector3 previous{};
		for (int segment = 0; segment <= kGoalLightningSegmentCount; ++segment) {
			const float t = static_cast<float>(segment) /
				static_cast<float>(kGoalLightningSegmentCount);
			const float remaining = 1.0f - t;
			const float angle = baseAngle + t * 2.7f;
			const float jitter = std::sin(
				static_cast<float>(segment) * 9.17f +
				static_cast<float>(branch) * 4.31f +
				elapsedSeconds_ * 13.0f) * 0.045f * remaining;
			const Vector3 point = center +
				horizontal * ((std::cos(angle) * 0.5f + jitter) * openingWidth * remaining) +
				Vector3{ 0.0f,
					(std::sin(angle) * 0.5f - jitter) * openingHeight * remaining,
					0.0f } +
				normal * (depth * 0.18f * remaining);
			if (segment > 0) {
				const float segmentMidpoint =
					(static_cast<float>(segment) - 0.5f) /
					static_cast<float>(kGoalLightningSegmentCount);
				const Vector4 color = WrappedDistance(segmentMidpoint, brightPosition) < 0.16f
					? kGoalLightningBrightColor
					: kGoalLightningColor;
				lineDrawer->DrawLine(previous, point, color);
			}
			previous = point;
		}
	}
}

void MagnetStageStructureVisualSystem::Draw(
	const MagnetStageData& stageData) const
{
	if (!ready_ || !object3dCommon_ ||
		stageData.goalCount > goalVisuals_.size() ||
		stageData.obstacleCount > wallVisuals_.size()) {
		return;
	}

	bool hasVisual = stageData.goalCount > 0;
	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		hasVisual = hasVisual || SupportsObstacle(stageData.obstacles[index].obstacleKind);
	}
	if (hasVisual) {
		object3dCommon_->BeginObjectPass();
		for (std::size_t index = 0; index < stageData.goalCount; ++index) {
			if (goalVisuals_[index]) {
				goalVisuals_[index]->Draw();
			}
		}
		for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
			if (SupportsObstacle(stageData.obstacles[index].obstacleKind) &&
				wallVisuals_[index]) {
				wallVisuals_[index]->Draw();
			}
		}
		object3dCommon_->EndObjectPass();
	}

	for (std::size_t index = 0; index < stageData.goalCount; ++index) {
		DrawGoalSuction(runtimeGoals_[index]);
	}
}

} // namespace magnet
