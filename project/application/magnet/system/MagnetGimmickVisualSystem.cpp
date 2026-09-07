#include "application/magnet/system/MagnetGimmickVisualSystem.h"

#include "application/magnet/system/MagnetChainSystem.h"
#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/PrimitiveGenerator.h"
#include "base/FrameClock.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <numbers>

namespace magnet {
namespace {

constexpr char kTransferFrameModelPath[] = "magnet/transfer/transfer.obj";
constexpr char kTransferFrameFilePath[] = "Resources/magnet/transfer/transfer.obj";
constexpr char kChainsawBodyModelPath[] = "magnet/chainsaw/chainsaw.obj";
constexpr char kChainsawBodyFilePath[] = "Resources/magnet/chainsaw/chainsaw.obj";
constexpr std::array<const char*, kChainsawVisualFrameCount> kChainsawChainModelPaths{
	"magnet/chainsaw/chainsaw_chain.obj",
	"magnet/chainsaw/chainsaw_chain_1.obj",
	"magnet/chainsaw/chainsaw_chain_2.obj",
	"magnet/chainsaw/chainsaw_chain_3.obj",
};
constexpr std::array<const char*, kChainsawVisualFrameCount> kChainsawChainFilePaths{
	"Resources/magnet/chainsaw/chainsaw_chain.obj",
	"Resources/magnet/chainsaw/chainsaw_chain_1.obj",
	"Resources/magnet/chainsaw/chainsaw_chain_2.obj",
	"Resources/magnet/chainsaw/chainsaw_chain_3.obj",
};
constexpr char kChainsawChainTexturePath[] = "Resources/magnet/chainsaw/chain_motion.png";
constexpr char kWhiteTexturePath[] = "Resources/human/white.png";
constexpr char kNoiseTexturePath[] = "Resources/noise1.png";
constexpr float kTau = std::numbers::pi_v<float> * 2.0f;
constexpr float kHalfPi = std::numbers::pi_v<float> * 0.5f;
constexpr float kTransferSourceWidth = 3.4f;
constexpr float kTransferSourceHeight = 3.2f;
constexpr float kTransferSourceDepth = 0.5f;
constexpr float kTransferOpeningWidthRatio = 2.6f / 3.4f;
constexpr float kTransferOpeningHeightRatio = 2.8f / 3.2f;
constexpr float kChainsawSourceWidth = 0.65f;
constexpr float kChainsawSourceHeight = 3.0f;
constexpr float kChainsawSourceThickness = 0.35f;
constexpr float kChainsawExposedRatio = 0.8f;
constexpr float kChainsawChainFramesPerSecond = 24.0f;
constexpr float kChainsawSparkLifetimeSeconds = 0.36f;
constexpr int kFieldSegments = 48;
constexpr int kAnchorSuctionWaveCount = 3;
constexpr int kAnchorFlowArrowCount = 8;
constexpr int kRepulsionWaveCount = 3;
constexpr int kRepulsionArrowCount = 8;
constexpr int kChainsawIdleSparkCount = 4;
constexpr int kChainsawCutSparkCount = 12;
constexpr Vector4 kAnchorCoreColor{ 0.22f, 0.05f, 0.34f, 1.0f };
constexpr Vector4 kAnchorRingColor{ 0.78f, 0.28f, 1.0f, 1.0f };
constexpr Vector4 kAnchorActiveColor{ 1.0f, 0.62f, 1.0f, 1.0f };
constexpr Vector4 kRepulsionCoreColor{ 0.34f, 0.025f, 0.16f, 1.0f };
constexpr Vector4 kRepulsionRingColor{ 1.0f, 0.16f, 0.58f, 1.0f };
constexpr Vector4 kChainsawIdleSparkColor{ 1.0f, 0.36f, 0.025f, 1.0f };
constexpr Vector4 kChainsawCutSparkColor{ 1.0f, 0.82f, 0.12f, 1.0f };
constexpr std::array<Vector4, 6> kTransferPairColors{
	Vector4{ 0.10f, 0.92f, 1.0f, 1.0f },
	Vector4{ 0.76f, 0.28f, 1.0f, 1.0f },
	Vector4{ 0.22f, 1.0f, 0.54f, 1.0f },
	Vector4{ 1.0f, 0.52f, 0.12f, 1.0f },
	Vector4{ 0.22f, 0.48f, 1.0f, 1.0f },
	Vector4{ 1.0f, 0.26f, 0.38f, 1.0f },
};

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

[[nodiscard]] Vector4 MultiplyRgb(const Vector4& color, float multiplier) noexcept
{
	return {
		std::clamp(color.x * multiplier, 0.0f, 1.0f),
		std::clamp(color.y * multiplier, 0.0f, 1.0f),
		std::clamp(color.z * multiplier, 0.0f, 1.0f),
		color.w,
	};
}

[[nodiscard]] Vector4 GetTransferPairColor(uint32_t pairId) noexcept
{
	if (pairId == 0) {
		return kTransferPairColors.front();
	}
	return kTransferPairColors[(pairId - 1u) % kTransferPairColors.size()];
}

[[nodiscard]] Vector3 GetChainsawFacing(
	const MagnetStageBoxPlacement& obstacle) noexcept
{
	return obstacle.size.z > obstacle.size.x
		? Vector3{ -1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 0.0f, -1.0f };
}

[[nodiscard]] Vector3 NormalizeHorizontalOr(
	const Vector3& value,
	const Vector3& fallback) noexcept
{
	const float lengthSquared = value.x * value.x + value.z * value.z;
	if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-8f) {
		return fallback;
	}
	const float inverseLength = 1.0f / std::sqrt(lengthSquared);
	return { value.x * inverseLength, 0.0f, value.z * inverseLength };
}

[[nodiscard]] float Hash01(uint32_t value) noexcept
{
	value ^= value >> 16u;
	value *= 0x7feb352du;
	value ^= value >> 15u;
	value *= 0x846ca68bu;
	value ^= value >> 16u;
	return static_cast<float>(value & 0x00ffffffu) /
		static_cast<float>(0x01000000u);
}

void DrawRingXZ(
	LineDrawer& lineDrawer,
	const Vector3& center,
	float radius,
	float angleOffset,
	const Vector4& color,
	int dashPhase = -1) noexcept
{
	if (!IsFinite(center) || !std::isfinite(radius) || radius <= 0.0f ||
		!std::isfinite(angleOffset)) {
		return;
	}
	for (int segment = 0; segment < kFieldSegments; ++segment) {
		if (dashPhase >= 0 && (segment + dashPhase) % 4 == 3) {
			continue;
		}
		const float firstAngle = angleOffset + kTau *
			static_cast<float>(segment) / static_cast<float>(kFieldSegments);
		const float secondAngle = angleOffset + kTau *
			static_cast<float>(segment + 1) / static_cast<float>(kFieldSegments);
		lineDrawer.DrawLine(
			center + Vector3{ std::cos(firstAngle) * radius, 0.0f,
				std::sin(firstAngle) * radius },
			center + Vector3{ std::cos(secondAngle) * radius, 0.0f,
				std::sin(secondAngle) * radius },
			color);
	}
}

} // namespace

MagnetGimmickVisualSystem::~MagnetGimmickVisualSystem() = default;

bool MagnetGimmickVisualSystem::Supports(MagnetObstacleKind kind) noexcept
{
	return kind == MagnetObstacleKind::TransferGate ||
		kind == MagnetObstacleKind::Chainsaw ||
		kind == MagnetObstacleKind::RepulsionField ||
		kind == MagnetObstacleKind::MagneticAnchor;
}

bool MagnetGimmickVisualSystem::Initialize(
	Object3dCommon* object3dCommon,
	ModelManager* modelManager,
	Camera* camera)
{
	Finalize();
	if (!object3dCommon || !modelManager || !camera ||
		!IsRegularFile(kTransferFrameFilePath) ||
		!IsRegularFile(kChainsawBodyFilePath) ||
		!IsRegularFile(kChainsawChainTexturePath) ||
		!IsRegularFile(kWhiteTexturePath) ||
		!IsRegularFile(kNoiseTexturePath)) {
		return false;
	}
	for (const char* framePath : kChainsawChainFilePaths) {
		if (!IsRegularFile(framePath)) {
			return false;
		}
	}

	object3dCommon_ = object3dCommon;
	modelManager->LoadModel(kTransferFrameModelPath);
	transferFrameModel_ = modelManager->GetModel(kTransferFrameModelPath);
	modelManager->LoadModel(kChainsawBodyModelPath);
	chainsawBodyModel_ = modelManager->GetModel(kChainsawBodyModelPath);
	if (chainsawBodyModel_) {
		chainsawBodyModel_->LoadTextures();
	}
	for (std::size_t frameIndex = 0;
		frameIndex < kChainsawChainModelPaths.size(); ++frameIndex) {
		modelManager->LoadModel(kChainsawChainModelPaths[frameIndex]);
		chainsawChainModels_[frameIndex] =
			modelManager->GetModel(kChainsawChainModelPaths[frameIndex]);
		if (chainsawChainModels_[frameIndex]) {
			chainsawChainModels_[frameIndex]->LoadTextures();
		}
	}
	portalModel_ = PrimitiveGenerator::CreateBox(modelManager, { 1.0f, 1.0f, 1.0f });
	coreModel_ = PrimitiveGenerator::CreateSphere(modelManager, 1.0f, 20);
	ringModel_ = PrimitiveGenerator::CreateRing(modelManager, 0.84f, 1.0f, 48);

	TextureManager* textureManager = TextureManager::GetInstance();
	whiteTexture_ = textureManager->LoadTexture2D(kWhiteTexturePath);
	noiseTexture_ = textureManager->LoadTexture2D(kNoiseTexturePath);
	if (!transferFrameModel_ || !chainsawBodyModel_ ||
		std::any_of(chainsawChainModels_.begin(), chainsawChainModels_.end(),
			[](const Model* model) noexcept { return model == nullptr; }) ||
		!portalModel_ || !coreModel_ || !ringModel_ ||
		!whiteTexture_.IsValid() || !noiseTexture_.IsValid()) {
		Finalize();
		return false;
	}

	for (VisualSlot& slot : slots_) {
		slot.primary = std::make_unique<Object3d>();
		slot.secondary = std::make_unique<Object3d>();
		slot.primary->Initialize(object3dCommon_);
		slot.secondary->Initialize(object3dCommon_);
		slot.configuredKind = MagnetObstacleKind::Count;
	}
	elapsedSeconds_ = 0.0f;
	ready_ = true;
	return true;
}

void MagnetGimmickVisualSystem::Finalize() noexcept
{
	ready_ = false;
	for (VisualSlot& slot : slots_) {
		slot.primary.reset();
		slot.secondary.reset();
		slot.configuredKind = MagnetObstacleKind::Count;
	}
	ringModel_.reset();
	coreModel_.reset();
	portalModel_.reset();
	transferFrameModel_ = nullptr;
	chainsawBodyModel_ = nullptr;
	chainsawChainModels_.fill(nullptr);
	chainsawSparkBursts_ = {};
	whiteTexture_ = {};
	noiseTexture_ = {};
	elapsedSeconds_ = 0.0f;
	nextChainsawSparkBurst_ = 0;
	object3dCommon_ = nullptr;
}

void MagnetGimmickVisualSystem::Reset() noexcept
{
	elapsedSeconds_ = 0.0f;
	chainsawSparkBursts_ = {};
	nextChainsawSparkBurst_ = 0;
}

bool MagnetGimmickVisualSystem::ConfigureSlot(
	VisualSlot& slot,
	MagnetObstacleKind kind) noexcept
{
	if (!slot.primary || !slot.secondary || !Supports(kind)) {
		return false;
	}

	Model* primaryModel = nullptr;
	Model* secondaryModel = nullptr;
	Texture2DHandle primaryTexture = whiteTexture_;
	Texture2DHandle secondaryTexture = whiteTexture_;
	bool primaryLighting = true;
	bool secondaryLighting = false;
	switch (kind) {
	case MagnetObstacleKind::TransferGate:
		primaryModel = transferFrameModel_;
		secondaryModel = portalModel_.get();
		secondaryTexture = noiseTexture_;
		break;
	case MagnetObstacleKind::Chainsaw:
		primaryModel = chainsawBodyModel_;
		secondaryModel = chainsawChainModels_.front();
		secondaryLighting = false;
		break;
	case MagnetObstacleKind::MagneticAnchor:
	case MagnetObstacleKind::RepulsionField:
		primaryModel = coreModel_.get();
		secondaryModel = ringModel_.get();
		break;
	default:
		return false;
	}
	if (!primaryModel || !secondaryModel ||
		!primaryTexture.IsValid() || !secondaryTexture.IsValid()) {
		return false;
	}

	slot.primary->SetModel(primaryModel);
	slot.primary->SetTexture(primaryTexture);
	slot.primary->SetEnableLighting(primaryLighting);
	slot.primary->SetShininess(72.0f);
	slot.primary->SetCullMode(2);
	slot.secondary->SetModel(secondaryModel);
	slot.secondary->SetTexture(secondaryTexture);
	slot.secondary->SetEnableLighting(secondaryLighting);
	slot.secondary->SetCullMode(0);
	if (!slot.primary->SetSurfaceTextureTransform({ 1.0f, 1.0f }, {}) ||
		!slot.secondary->SetSurfaceTextureTransform({ 1.0f, 1.0f }, {})) {
		return false;
	}
	slot.configuredKind = kind;
	return true;
}

bool MagnetGimmickVisualSystem::UpdateTransferGate(
	VisualSlot& slot,
	const MagnetStageBoxPlacement& obstacle,
	Camera* camera,
	float deltaTime) noexcept
{
	const bool normalAlongX = obstacle.size.x <= obstacle.size.z;
	const float frameWidth = normalAlongX ? obstacle.size.z : obstacle.size.x;
	const float frameDepth = normalAlongX ? obstacle.size.x : obstacle.size.z;
	const Vector3 frameScale{
		frameWidth / kTransferSourceWidth,
		obstacle.size.y / kTransferSourceHeight,
		frameDepth / kTransferSourceDepth,
	};
	const Vector4 pairColor = GetTransferPairColor(obstacle.transferPairId);
	const float phase = static_cast<float>(obstacle.id % 29u) * 0.31f;
	const float pulse = 0.78f + 0.22f *
		(0.5f + 0.5f * std::sin(elapsedSeconds_ * 3.1f + phase));

	Object3d& frame = *slot.primary;
	frame.SetPosition({
		obstacle.position.x,
		obstacle.position.y - obstacle.size.y * 0.5f,
		obstacle.position.z,
	});
	frame.SetRotation({ 0.0f, normalAlongX ? kHalfPi : 0.0f, 0.0f });
	frame.SetColor(MultiplyRgb(pairColor, 0.38f + pulse * 0.16f));
	if (!frame.SetScale(frameScale)) {
		return false;
	}
	frame.Update(camera, deltaTime);

	const float openingWidth = frameWidth * kTransferOpeningWidthRatio * 0.96f;
	const float openingHeight = obstacle.size.y * kTransferOpeningHeightRatio * 0.96f;
	const float portalDepth = (std::max)(frameDepth * 0.12f, 0.02f);
	Object3d& portal = *slot.secondary;
	portal.SetPosition({
		obstacle.position.x,
		obstacle.position.y - obstacle.size.y * 0.5f + openingHeight * 0.5f,
		obstacle.position.z,
	});
	portal.SetRotation({});
	portal.SetColor(MultiplyRgb(pairColor, 0.82f + pulse * 0.18f));
	const Vector3 portalScale = normalAlongX
		? Vector3{ portalDepth, openingHeight, openingWidth }
		: Vector3{ openingWidth, openingHeight, portalDepth };
	if (!portal.SetScale(portalScale) ||
		!portal.SetSurfaceTextureTransform(
			{ 2.2f, 2.8f },
			{ elapsedSeconds_ * 0.16f + phase, -elapsedSeconds_ * 0.11f },
			Object3d::SurfaceMappingMode::ModelUv)) {
		return false;
	}
	portal.Update(camera, deltaTime);
	return true;
}

bool MagnetGimmickVisualSystem::UpdateChainsaw(
	VisualSlot& slot,
	const MagnetStageBoxPlacement& obstacle,
	Camera* camera,
	float deltaTime) noexcept
{
	const bool widthAlongZ = obstacle.size.z > obstacle.size.x;
	const float authoredWidth = widthAlongZ ? obstacle.size.z : obstacle.size.x;
	const float authoredThickness = widthAlongZ ? obstacle.size.x : obstacle.size.z;
	const Vector3 scale{
		authoredWidth / kChainsawSourceWidth,
		obstacle.size.y * kChainsawExposedRatio / kChainsawSourceHeight,
		authoredThickness / kChainsawSourceThickness,
	};
	const Vector3 rotation{ 0.0f, widthAlongZ ? -kHalfPi : 0.0f, 0.0f };
	const Vector3 visualPosition{
		obstacle.position.x,
		obstacle.position.y +
			obstacle.size.y * (1.0f - kChainsawExposedRatio) * 0.5f,
		obstacle.position.z,
	};

	Object3d& body = *slot.primary;
	body.SetPosition(visualPosition);
	body.SetRotation(rotation);
	body.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	if (!body.SetScale(scale) ||
		!body.SetSurfaceTextureTransform({ 1.0f, 1.0f }, {})) {
		return false;
	}
	body.Update(camera, deltaTime);

	const float chainPulse = 0.86f + 0.14f *
		std::sin(elapsedSeconds_ * 18.0f + static_cast<float>(obstacle.id));
	float visualFrame = std::fmod(
		elapsedSeconds_ * kChainsawChainFramesPerSecond,
		static_cast<float>(chainsawChainModels_.size()));
	if (visualFrame < 0.0f) {
		visualFrame += static_cast<float>(chainsawChainModels_.size());
	}
	const std::size_t frameIndex = static_cast<std::size_t>(visualFrame);
	Model* chainFrameModel = chainsawChainModels_[frameIndex];
	if (!chainFrameModel) {
		return false;
	}
	Object3d& chain = *slot.secondary;
	chain.SetPosition(visualPosition);
	chain.SetRotation(rotation);
	chain.SetColor({ 1.0f, chainPulse, chainPulse * 0.82f, 1.0f });
	if (!chain.TrySwapStaticModel(chainFrameModel) ||
		!chain.SetScale(scale) ||
		!chain.SetSurfaceTextureTransform(
			{ 1.0f, 1.0f }, { -elapsedSeconds_ * 3.6f, 0.0f })) {
		return false;
	}
	chain.Update(camera, deltaTime);
	return true;
}

bool MagnetGimmickVisualSystem::UpdateMagneticAnchor(
	VisualSlot& slot,
	std::size_t obstacleIndex,
	const MagnetStageBoxPlacement& obstacle,
	const MagnetChainSystem& chainSystem,
	Camera* camera,
	float deltaTime) noexcept
{
	const float horizontalRadius = (std::max)(obstacle.size.x, obstacle.size.z) * 0.5f;
	const float coreRadius = (std::max)(0.02f,
		(std::min)(horizontalRadius * 0.36f, obstacle.size.y * 0.38f));
	const physics::BodyHandle anchoredHandle = chainSystem.GetAnchoredBody(obstacleIndex);
	const physics::SphereBody* anchoredBody =
		chainSystem.GetPhysicsWorld().GetBody(anchoredHandle);
	const bool active = anchoredBody && anchoredBody->active;
	const float pulse = 0.86f + 0.14f *
		std::sin(elapsedSeconds_ * (active ? 7.0f : 2.4f));

	Object3d& core = *slot.primary;
	core.SetPosition(obstacle.position);
	core.SetRotation({ 0.0f, elapsedSeconds_ * 0.28f, 0.0f });
	core.SetColor(active ? MultiplyRgb(kAnchorActiveColor, pulse) : kAnchorCoreColor);
	if (!core.SetScale({ coreRadius, coreRadius, coreRadius })) {
		return false;
	}
	core.Update(camera, deltaTime);

	const float orbitRadius = (std::max)(coreRadius * 1.55f, 0.04f);
	Object3d& ring = *slot.secondary;
	ring.SetPosition(obstacle.position);
	ring.SetRotation({ 0.34f, elapsedSeconds_ * 1.15f, 0.0f });
	ring.SetColor(active ? kAnchorActiveColor : MultiplyRgb(kAnchorRingColor, pulse));
	if (!ring.SetScale({ orbitRadius, orbitRadius, orbitRadius })) {
		return false;
	}
	ring.Update(camera, deltaTime);
	return true;
}

bool MagnetGimmickVisualSystem::UpdateRepulsionField(
	VisualSlot& slot,
	const MagnetStageBoxPlacement& obstacle,
	const MagnetChainSystem& chainSystem,
	Camera* camera,
	float deltaTime) noexcept
{
	const float fieldRadius = chainSystem.GetRepulsionFieldRadius(obstacle);
	if (!std::isfinite(fieldRadius) || fieldRadius <= 0.0f) {
		return false;
	}
	const float baseY = obstacle.position.y - obstacle.size.y * 0.5f;
	const float coreRadius = (std::max)(0.02f,
		(std::min)(fieldRadius * 0.11f, obstacle.size.y * 0.28f));
	const float pulse = 0.90f + 0.10f * std::sin(elapsedSeconds_ * 4.2f);

	Object3d& core = *slot.primary;
	core.SetPosition({ obstacle.position.x, baseY + coreRadius, obstacle.position.z });
	core.SetRotation({ 0.0f, -elapsedSeconds_ * 0.42f, 0.0f });
	core.SetColor(MultiplyRgb(kRepulsionCoreColor, pulse));
	if (!core.SetScale({ coreRadius, coreRadius, coreRadius })) {
		return false;
	}
	core.Update(camera, deltaTime);

	Object3d& ring = *slot.secondary;
	ring.SetPosition({ obstacle.position.x, baseY + 0.045f, obstacle.position.z });
	ring.SetRotation({ kHalfPi, 0.0f, elapsedSeconds_ * 0.18f });
	ring.SetColor(MultiplyRgb(kRepulsionRingColor, pulse));
	if (!ring.SetScale({ fieldRadius, fieldRadius, fieldRadius })) {
		return false;
	}
	ring.Update(camera, deltaTime);
	return true;
}

void MagnetGimmickVisualSystem::AddChainsawCutEffect(
	const MagnetStageData& stageData,
	const MagnetChainSystem& chainSystem) noexcept
{
	if (!ready_ || stageData.obstacleCount > stageData.obstacles.size()) {
		return;
	}
	const MagnetChainSystem::ChainsawCutEvent& event =
		chainSystem.GetChainsawCutEvent();
	if (!event.occurred || !event.contactedBody.IsValid()) {
		return;
	}
	const physics::SphereBody* body =
		chainSystem.GetPhysicsWorld().GetBody(event.contactedBody);
	if (!body || !IsFinite(body->position)) {
		return;
	}
	const MagnetStageBoxPlacement* chainsaw = nullptr;
	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		if (obstacle.id == event.obstacleId &&
			obstacle.obstacleKind == MagnetObstacleKind::Chainsaw &&
			IsFinite(obstacle.position) && IsPositiveFiniteSize(obstacle.size)) {
			chainsaw = &obstacle;
			break;
		}
	}
	if (!chainsaw) {
		return;
	}

	ChainsawSparkBurst& burst =
		chainsawSparkBursts_[nextChainsawSparkBurst_ % chainsawSparkBursts_.size()];
	burst.position = body->position;
	burst.outward = NormalizeHorizontalOr(
		body->position - chainsaw->position,
		GetChainsawFacing(*chainsaw));
	burst.ageSeconds = 0.0f;
	burst.seed = event.obstacleId * 0x9e3779b9u ^
		event.contactedBody.index * 0x85ebca6bu ^
		static_cast<uint32_t>(nextChainsawSparkBurst_);
	burst.active = true;
	nextChainsawSparkBurst_ =
		(nextChainsawSparkBurst_ + 1u) % chainsawSparkBursts_.size();
}

bool MagnetGimmickVisualSystem::Update(
	float deltaTime,
	const MagnetStageData& stageData,
	const MagnetChainSystem& chainSystem,
	Camera* camera) noexcept
{
	if (!ready_ || !object3dCommon_ || !camera ||
		stageData.obstacleCount > slots_.size() ||
		!std::isfinite(deltaTime) || deltaTime < 0.0f) {
		return false;
	}
	deltaTime = (std::min)(deltaTime, FrameClock::kMaximumFrameDeltaSeconds);
	elapsedSeconds_ = std::remainder(elapsedSeconds_ + deltaTime, 4096.0f);
	for (ChainsawSparkBurst& burst : chainsawSparkBursts_) {
		if (!burst.active) {
			continue;
		}
		burst.ageSeconds += deltaTime;
		if (!std::isfinite(burst.ageSeconds) ||
			burst.ageSeconds >= kChainsawSparkLifetimeSeconds) {
			burst = {};
		}
	}

	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		if (!Supports(obstacle.obstacleKind)) {
			continue;
		}
		if (!IsFinite(obstacle.position) || !IsPositiveFiniteSize(obstacle.size)) {
			return false;
		}
		VisualSlot& slot = slots_[index];
		if (slot.configuredKind != obstacle.obstacleKind &&
			!ConfigureSlot(slot, obstacle.obstacleKind)) {
			return false;
		}
		bool updated = false;
		switch (obstacle.obstacleKind) {
		case MagnetObstacleKind::TransferGate:
			updated = UpdateTransferGate(slot, obstacle, camera, deltaTime);
			break;
		case MagnetObstacleKind::Chainsaw:
			updated = UpdateChainsaw(slot, obstacle, camera, deltaTime);
			break;
		case MagnetObstacleKind::MagneticAnchor:
			updated = UpdateMagneticAnchor(
				slot, index, obstacle, chainSystem, camera, deltaTime);
			break;
		case MagnetObstacleKind::RepulsionField:
			updated = UpdateRepulsionField(
				slot, obstacle, chainSystem, camera, deltaTime);
			break;
		default:
			return false;
		}
		if (!updated) {
			return false;
		}
	}
	return true;
}

void MagnetGimmickVisualSystem::Draw(
	const MagnetStageData& stageData,
	const MagnetChainSystem& chainSystem) const
{
	if (!ready_ || !object3dCommon_ || stageData.obstacleCount > slots_.size()) {
		return;
	}

	bool hasObjectVisual = false;
	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		if (Supports(stageData.obstacles[index].obstacleKind) &&
			slots_[index].configuredKind == stageData.obstacles[index].obstacleKind) {
			hasObjectVisual = true;
			break;
		}
	}
	if (hasObjectVisual) {
		object3dCommon_->BeginObjectPass();
		for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
			const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
			const VisualSlot& slot = slots_[index];
			if (Supports(obstacle.obstacleKind) &&
				slot.configuredKind == obstacle.obstacleKind &&
				slot.primary && slot.secondary) {
				slot.primary->Draw();
				slot.secondary->Draw();
			}
		}
		object3dCommon_->EndObjectPass();
	}

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	if (!lineDrawer) {
		return;
	}
	for (const ChainsawSparkBurst& burst : chainsawSparkBursts_) {
		if (!burst.active || !IsFinite(burst.position) || !IsFinite(burst.outward) ||
			!std::isfinite(burst.ageSeconds)) {
			continue;
		}
		const float lifeRatio = std::clamp(
			burst.ageSeconds / kChainsawSparkLifetimeSeconds, 0.0f, 1.0f);
		const Vector3 tangent{ -burst.outward.z, 0.0f, burst.outward.x };
		for (int spark = 0; spark < kChainsawCutSparkCount; ++spark) {
			const uint32_t sparkSeed = burst.seed +
				static_cast<uint32_t>(spark) * 0x9e3779b9u;
			const float spread = Hash01(sparkSeed) * 2.0f - 1.0f;
			const float speed = 1.8f + Hash01(sparkSeed ^ 0x68bc21ebu) * 2.6f;
			const float upward = 0.32f + Hash01(sparkSeed ^ 0x02e5be93u) * 0.78f;
			const Vector3 horizontal = NormalizeHorizontalOr(
				burst.outward + tangent * spread * 1.15f,
				burst.outward);
			Vector3 head = burst.position + horizontal * (speed * burst.ageSeconds);
			head.y += upward * speed * burst.ageSeconds -
				4.8f * burst.ageSeconds * burst.ageSeconds;
			const float trailLength =
				(0.12f + Hash01(sparkSeed ^ 0xa511e9b3u) * 0.24f) *
				(1.0f - lifeRatio);
			const Vector3 tail = head - Vector3{
				horizontal.x * trailLength,
				upward * trailLength,
				horizontal.z * trailLength,
			};
			Vector4 color = kChainsawCutSparkColor;
			color.y = 0.82f - lifeRatio * 0.46f;
			color.w = 1.0f - lifeRatio;
			lineDrawer->DrawLine(tail, head, color);
		}
	}
	for (std::size_t index = 0; index < stageData.obstacleCount; ++index) {
		const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		if (obstacle.obstacleKind == MagnetObstacleKind::Chainsaw &&
			slots_[index].configuredKind == MagnetObstacleKind::Chainsaw &&
			IsFinite(obstacle.position) && IsPositiveFiniteSize(obstacle.size)) {
			const Vector3 forward = GetChainsawFacing(obstacle);
			const Vector3 tangent{ -forward.z, 0.0f, forward.x };
			const float width = (std::max)(obstacle.size.x, obstacle.size.z);
			const Vector3 bladeTip = obstacle.position +
				Vector3{ 0.0f, obstacle.size.y * 0.47f, 0.0f };
			for (int spark = 0; spark < kChainsawIdleSparkCount; ++spark) {
				const float rawPhase = std::fmod(
					elapsedSeconds_ * 2.2f +
					static_cast<float>(spark) /
					static_cast<float>(kChainsawIdleSparkCount),
					1.0f);
				if (rawPhase >= 0.62f) {
					continue;
				}
				const float phase = rawPhase / 0.62f;
				const float side =
					(static_cast<float>((spark * 5) % 7) / 3.0f - 1.0f) * 0.52f;
				const Vector3 direction = NormalizeHorizontalOr(
					forward + tangent * side,
					forward);
				Vector3 head = bladeTip + direction * (width * 0.48f * phase);
				head.y += obstacle.size.y *
					(0.02f + phase * 0.07f - phase * phase * 0.06f);
				const Vector3 tail = head - direction *
					(width * 0.14f * (1.0f - phase));
				Vector4 color = kChainsawIdleSparkColor;
				color.w = 1.0f - phase;
				lineDrawer->DrawLine(tail, head, color);
			}
		}
		if (obstacle.obstacleKind == MagnetObstacleKind::MagneticAnchor) {
			const float radius = chainSystem.GetAnchorAttractionRadius(obstacle);
			if (!std::isfinite(radius) || radius <= 0.0f) {
				continue;
			}
			const Vector3 ringCenter{
				obstacle.position.x,
				obstacle.position.y - obstacle.size.y * 0.5f + 0.035f,
				obstacle.position.z,
			};
			const int dashPhase = static_cast<int>(elapsedSeconds_ * 7.0f) % 4;
			DrawRingXZ(*lineDrawer, ringCenter, radius,
				elapsedSeconds_ * 0.22f, kAnchorRingColor, dashPhase);
			const physics::BodyHandle anchoredHandle = chainSystem.GetAnchoredBody(index);
			const physics::SphereBody* anchoredBody =
				chainSystem.GetPhysicsWorld().GetBody(anchoredHandle);
			const bool hasActiveBody = anchoredBody && anchoredBody->active &&
				IsFinite(anchoredBody->position);
			const float suctionSpeed = hasActiveBody ? 1.15f : 0.58f;
			for (int wave = 0; wave < kAnchorSuctionWaveCount; ++wave) {
				const float phase = std::fmod(
					elapsedSeconds_ * suctionSpeed +
					static_cast<float>(wave) /
					static_cast<float>(kAnchorSuctionWaveCount),
					1.0f);
				const float waveRadius = radius * (0.90f - phase * 0.68f);
				Vector4 waveColor = hasActiveBody ? kAnchorActiveColor : kAnchorRingColor;
				waveColor.w = 0.18f + phase * (hasActiveBody ? 0.72f : 0.46f);
				DrawRingXZ(*lineDrawer, ringCenter, waveRadius,
					elapsedSeconds_ * -0.10f, waveColor);
			}
			for (int arrow = 0; arrow < kAnchorFlowArrowCount; ++arrow) {
				const float phase = std::fmod(
					elapsedSeconds_ * suctionSpeed +
					static_cast<float>(arrow) /
					static_cast<float>(kAnchorFlowArrowCount),
					1.0f);
				const float angle = kTau * static_cast<float>(arrow) /
					static_cast<float>(kAnchorFlowArrowCount) -
					elapsedSeconds_ * 0.08f;
				const Vector3 outward{ std::cos(angle), 0.0f, std::sin(angle) };
				const Vector3 tangent{ -outward.z, 0.0f, outward.x };
				const float tipRadius = radius * (0.88f - phase * 0.66f);
				const float trailLength = radius * (hasActiveBody ? 0.17f : 0.12f);
				const Vector3 tip = ringCenter + outward * tipRadius;
				const Vector3 tail = ringCenter + outward *
					(std::min)(radius * 0.96f, tipRadius + trailLength);
				const Vector3 arrowBase = tip + outward * (radius * 0.075f);
				const Vector3 arrowWidth = tangent * (radius * 0.035f);
				Vector4 flowColor = hasActiveBody ? kAnchorActiveColor : kAnchorRingColor;
				flowColor.w = 0.34f + phase * (hasActiveBody ? 0.66f : 0.42f);
				lineDrawer->DrawLine(tail, tip, flowColor);
				lineDrawer->DrawLine(arrowBase + arrowWidth, tip, flowColor);
				lineDrawer->DrawLine(arrowBase - arrowWidth, tip, flowColor);
			}
			if (hasActiveBody) {
				lineDrawer->DrawLine(obstacle.position, anchoredBody->position,
					kAnchorActiveColor);
			}
		}
		if (obstacle.obstacleKind == MagnetObstacleKind::RepulsionField) {
			const float radius = chainSystem.GetRepulsionFieldRadius(obstacle);
			if (!std::isfinite(radius) || radius <= 0.0f) {
				continue;
			}
			const Vector3 center{
				obstacle.position.x,
				obstacle.position.y - obstacle.size.y * 0.5f + 0.055f,
				obstacle.position.z,
			};
			for (int wave = 0; wave < kRepulsionWaveCount; ++wave) {
				const float phase = std::fmod(
					elapsedSeconds_ * 0.42f +
					static_cast<float>(wave) / static_cast<float>(kRepulsionWaveCount),
					1.0f);
				const float waveRadius = radius * (0.18f + phase * 0.82f);
				Vector4 waveColor = kRepulsionRingColor;
				waveColor.w = 1.0f - phase;
				DrawRingXZ(*lineDrawer, center, waveRadius,
					-elapsedSeconds_ * 0.18f, waveColor);
			}
			for (int arrow = 0; arrow < kRepulsionArrowCount; ++arrow) {
				const float angle = kTau * static_cast<float>(arrow) /
					static_cast<float>(kRepulsionArrowCount) + elapsedSeconds_ * 0.12f;
				const Vector3 direction{ std::cos(angle), 0.0f, std::sin(angle) };
				const Vector3 tangent{ -direction.z, 0.0f, direction.x };
				const Vector3 start = center + direction * (radius * 0.30f);
				const Vector3 end = center + direction * (radius * 0.78f);
				const Vector3 arrowBase = end - direction * (radius * 0.10f);
				const Vector3 arrowWidth = tangent * (radius * 0.045f);
				lineDrawer->DrawLine(start, end, kRepulsionRingColor);
				lineDrawer->DrawLine(arrowBase + arrowWidth, end, kRepulsionRingColor);
				lineDrawer->DrawLine(arrowBase - arrowWidth, end, kRepulsionRingColor);
			}
		}
	}
}

} // namespace magnet
