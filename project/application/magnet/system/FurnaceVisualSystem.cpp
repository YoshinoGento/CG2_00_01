#include "application/magnet/system/FurnaceVisualSystem.h"

#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/PrimitiveGenerator.h"
#include "base/FrameClock.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace magnet {
namespace {

constexpr char kMagmaTexturePath[] = "Resources/magnet/furnace/magma_base.png";
constexpr char kNoiseTexturePath[] = "Resources/noise1.png";
constexpr char kWhiteTexturePath[] = "Resources/human/white.png";
constexpr float kEffectDurationSeconds = 0.46f;
constexpr float kFurnaceBaseThreshold = 0.10f;
constexpr float kFurnaceThresholdAmplitude = 0.035f;
constexpr float kFurnaceAnimationRate = 1.15f;
constexpr Vector2 kMagmaWorldUvScale{ 0.62f, 0.62f };
constexpr Vector2 kMagmaFlowVelocity{ 0.065f, -0.095f };
constexpr Vector4 kFurnaceEdgeColor{ 1.0f, 0.72f, 0.08f, 1.0f };
constexpr Vector4 kBallDissolveColor{ 1.0f, 0.24f, 0.04f, 1.0f };

[[nodiscard]] bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

[[nodiscard]] bool IsPositiveFiniteSize(const Vector3& size) noexcept
{
	return IsFinite(size) && size.x > 0.0f && size.y > 0.0f && size.z > 0.0f;
}

[[nodiscard]] bool IsRegularFile(const char* path) noexcept
{
	std::error_code error;
	return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] float SmoothStep01(float value) noexcept
{
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}

} // namespace

FurnaceVisualSystem::~FurnaceVisualSystem() = default;

bool FurnaceVisualSystem::Initialize(
	Object3dCommon* object3dCommon,
	ModelManager* modelManager,
	Camera* camera)
{
	ready_ = false;
	object3dCommon_ = object3dCommon;
	furnaceModel_.reset();
	dissolveBallModel_.reset();
	for (auto& visual : furnaceVisuals_) { visual.reset(); }
	for (auto& visual : dissolveVisuals_) { visual.reset(); }
	Reset();

	if (!object3dCommon_ || !modelManager || !camera ||
		!IsRegularFile(kMagmaTexturePath) ||
		!IsRegularFile(kNoiseTexturePath) ||
		!IsRegularFile(kWhiteTexturePath)) {
		return false;
	}

	TextureManager* textureManager = TextureManager::GetInstance();
	magmaTexture_ = textureManager->LoadTexture2D(kMagmaTexturePath);
	noiseTexture_ = textureManager->LoadTexture2D(kNoiseTexturePath);
	whiteTexture_ = textureManager->LoadTexture2D(kWhiteTexturePath);
	if (!magmaTexture_.IsValid() || !noiseTexture_.IsValid() ||
		!whiteTexture_.IsValid()) {
		return false;
	}

	furnaceModel_ = PrimitiveGenerator::CreateBox(modelManager, { 1.0f, 1.0f, 1.0f });
	dissolveBallModel_ = PrimitiveGenerator::CreateSphere(modelManager, 1.0f, 16);
	if (!furnaceModel_ || !dissolveBallModel_) {
		return false;
	}

	const auto createVisual = [&](Model* model, Texture2DHandle texture) {
		auto visual = std::make_unique<Object3d>();
		visual->Initialize(object3dCommon_);
		visual->SetModel(model);
		visual->SetTexture(texture);
		visual->SetDissolveMask(noiseTexture_);
		visual->SetEnableLighting(false);
		return visual;
	};
	for (auto& visual : furnaceVisuals_) {
		visual = createVisual(furnaceModel_.get(), magmaTexture_);
	}
	for (auto& visual : dissolveVisuals_) {
		visual = createVisual(dissolveBallModel_.get(), whiteTexture_);
		visual->SetColor(kBallDissolveColor);
	}

	ready_ = Update(0.0f, {}, camera);
	return ready_;
}

void FurnaceVisualSystem::Finalize() noexcept
{
	ready_ = false;
	Reset();
	for (auto& visual : dissolveVisuals_) { visual.reset(); }
	for (auto& visual : furnaceVisuals_) { visual.reset(); }
	dissolveBallModel_.reset();
	furnaceModel_.reset();
	magmaTexture_ = {};
	whiteTexture_ = {};
	noiseTexture_ = {};
	object3dCommon_ = nullptr;
}

void FurnaceVisualSystem::Reset() noexcept
{
	dissolveEffects_.fill({});
	elapsedSeconds_ = 0.0f;
}

bool FurnaceVisualSystem::AddDissolveEvents(
	const MagnetChainSystem::FurnaceDissolveEvents& events,
	std::size_t eventCount) noexcept
{
	if (!ready_ || eventCount > events.size()) {
		return false;
	}
	for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
		const MagnetChainSystem::FurnaceDissolveEvent& event = events[eventIndex];
		if (!IsFinite(event.position) || !std::isfinite(event.radius) ||
			event.radius <= 0.0f || event.obstacleId == 0) {
			return false;
		}

		std::size_t targetIndex = dissolveEffects_.size();
		for (std::size_t index = 0; index < dissolveEffects_.size(); ++index) {
			if (!dissolveEffects_[index].active) {
				targetIndex = index;
				break;
			}
		}
		if (targetIndex >= dissolveEffects_.size()) {
			targetIndex = static_cast<std::size_t>(std::max_element(
				dissolveEffects_.begin(), dissolveEffects_.end(),
				[](const DissolveEffect& left, const DissolveEffect& right) {
					return left.ageSeconds < right.ageSeconds;
				}) - dissolveEffects_.begin());
		}
		dissolveEffects_[targetIndex] = {
			event.position,
			event.radius,
			0.0f,
			true,
		};
	}
	return true;
}

bool FurnaceVisualSystem::Update(
	float deltaTime,
	const MagnetStageData& stageData,
	Camera* camera) noexcept
{
	if (!object3dCommon_ || !furnaceModel_ || !camera ||
		stageData.obstacleCount > furnaceVisuals_.size() ||
		!std::isfinite(deltaTime) || deltaTime < 0.0f) {
		return false;
	}
	deltaTime = std::min(deltaTime, FrameClock::kMaximumFrameDeltaSeconds);
	elapsedSeconds_ = std::remainder(
		elapsedSeconds_ + deltaTime, 4096.0f);

	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		if (obstacle.obstacleKind != MagnetObstacleKind::Furnace) {
			continue;
		}
		if (!furnaceVisuals_[index] || !IsFinite(obstacle.position) ||
			!IsPositiveFiniteSize(obstacle.size)) {
			return false;
		}
		Object3d& visual = *furnaceVisuals_[index];
		visual.SetPosition(obstacle.position);
		if (!visual.SetScale(obstacle.size)) {
			return false;
		}
		const float phase = static_cast<float>(obstacle.id % 31u) * 0.37f;
		const Vector2 magmaUvOffset{
			elapsedSeconds_ * kMagmaFlowVelocity.x + phase * 0.17f,
			elapsedSeconds_ * kMagmaFlowVelocity.y + phase * 0.11f,
		};
		if (!visual.SetSurfaceTextureTransform(
			kMagmaWorldUvScale,
			magmaUvOffset,
			Object3d::SurfaceMappingMode::TriplanarWorld)) {
			return false;
		}
		Object3d::DissolveSettings settings{};
		settings.enabled = true;
		settings.threshold = kFurnaceBaseThreshold +
			std::sin(elapsedSeconds_ * kFurnaceAnimationRate + phase) *
			kFurnaceThresholdAmplitude;
		settings.edgeWidth = 0.095f;
		settings.edgeIntensity = 1.8f;
		settings.edgeColor = kFurnaceEdgeColor;
		settings.noiseUvScale = { 2.1f, 2.1f };
		settings.noiseUvOffset = {
			elapsedSeconds_ * 0.035f + phase,
			-elapsedSeconds_ * 0.052f + phase * 0.5f,
		};
		if (!visual.SetDissolveSettings(settings)) {
			return false;
		}
		visual.Update(camera, deltaTime);
	}

	for (std::size_t index = 0; index < dissolveEffects_.size(); ++index) {
		DissolveEffect& effect = dissolveEffects_[index];
		if (!effect.active) {
			continue;
		}
		if (!dissolveVisuals_[index]) {
			return false;
		}
		effect.ageSeconds += deltaTime;
		if (!std::isfinite(effect.ageSeconds) ||
			effect.ageSeconds >= kEffectDurationSeconds) {
			effect = {};
			continue;
		}
		const float progress = SmoothStep01(
			effect.ageSeconds / kEffectDurationSeconds);
		Object3d& visual = *dissolveVisuals_[index];
		visual.SetPosition(effect.position + Vector3{ 0.0f, progress * 0.12f, 0.0f });
		const float scale = effect.radius * (1.0f + progress * 0.18f);
		if (!visual.SetScale({ scale, scale, scale })) {
			return false;
		}
		Object3d::DissolveSettings settings{};
		settings.enabled = true;
		settings.threshold = std::min(progress * 1.02f, 1.0f);
		settings.edgeWidth = 0.16f;
		settings.edgeIntensity = 2.4f;
		settings.edgeColor = kFurnaceEdgeColor;
		settings.noiseUvScale = { 1.65f, 1.65f };
		settings.noiseUvOffset = {
			elapsedSeconds_ * 0.18f + static_cast<float>(index) * 0.23f,
			-elapsedSeconds_ * 0.13f,
		};
		if (!visual.SetDissolveSettings(settings)) {
			return false;
		}
		visual.Update(camera, deltaTime);
	}
	return true;
}

void FurnaceVisualSystem::Draw(const MagnetStageData& stageData) const
{
	if (!ready_ || !object3dCommon_ ||
		stageData.obstacleCount > furnaceVisuals_.size()) {
		return;
	}
	object3dCommon_->BeginObjectPass();
	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		if (stageData.obstacles[index].obstacleKind == MagnetObstacleKind::Furnace &&
			furnaceVisuals_[index]) {
			furnaceVisuals_[index]->Draw();
		}
	}
	for (std::size_t index = 0; index < dissolveEffects_.size(); ++index) {
		if (dissolveEffects_[index].active && dissolveVisuals_[index]) {
			dissolveVisuals_[index]->Draw();
		}
	}
	object3dCommon_->EndObjectPass();
}

std::size_t FurnaceVisualSystem::GetActiveEffectCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		dissolveEffects_.begin(), dissolveEffects_.end(),
		[](const DissolveEffect& effect) { return effect.active; }));
}

} // namespace magnet
