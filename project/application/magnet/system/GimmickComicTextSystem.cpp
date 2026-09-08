#include "application/magnet/system/GimmickComicTextSystem.h"

#include "application/magnet/system/MagnetChainSystem.h"

#include <algorithm>

namespace {
bool SameBody(physics::BodyHandle left, physics::BodyHandle right) noexcept {
	return left.index == right.index && left.generation == right.generation;
}

float DistanceSquaredXZ(const Vector3& left, const Vector3& right) noexcept {
	const float x = left.x - right.x;
	const float z = left.z - right.z;
	return x * x + z * z;
}

const magnet::MagnetStageBoxPlacement* FindObstacle(
	const magnet::MagnetStageData& stage, uint32_t id) noexcept {
	for (std::size_t index = 0; index < stage.obstacleCount; ++index) {
		if (stage.obstacles[index].id == id) { return &stage.obstacles[index]; }
	}
	return nullptr;
}

ComicTextEffectPreset MakeFallback(
	const char* text, float scale, const Vector4& textColor,
	const Vector4& extrusionColor, float rotation) {
	ComicTextEffectPreset preset{};
	preset.text = text;
	preset.textScale = scale;
	preset.characterSpacing = -10.0f;
	preset.textColor = textColor;
	preset.extrusionColor = extrusionColor;
	preset.extrusionOffset = { 4.0f, 5.0f };
	preset.screenOffset = { 0.0f, -58.0f };
	preset.drift = { 0.0f, -18.0f };
	preset.duration = 0.58f;
	preset.startScale = 0.14f;
	preset.peakScale = 1.08f;
	preset.endScale = 0.84f;
	preset.popFraction = 0.2f;
	preset.fadeFraction = 0.38f;
	preset.rotation = rotation;
	preset.shakeAmplitude = 4.5f;
	preset.shakeFrequency = 32.0f;
	return preset;
}
} // namespace

namespace magnet {
bool GimmickComicTextSystem::Initialize(ComicTextEffectSystem* effects) {
	Finalize();
	effects_ = effects;
	const std::array<const char*, static_cast<std::size_t>(Preset::Count)> names{
		"チェーンソー", "バンパー", "溶鉱炉", "磁石アンカー",
		"開閉シャッター", "転送ゲート", "反発磁場" };
	presets_ = {
		MakeFallback("ズバギャァン！！", 0.44f, { 1.0f, 0.88f, 0.28f, 1.0f }, { 0.72f, 0.04f, 0.02f, 1.0f }, -0.12f),
		MakeFallback("ボインッ！", 0.38f, { 0.45f, 0.95f, 1.0f, 1.0f }, { 0.02f, 0.25f, 0.8f, 1.0f }, 0.05f),
		MakeFallback("ジュイーーーーンッ！！！", 0.36f, { 1.0f, 0.55f, 0.08f, 1.0f }, { 0.72f, 0.03f, 0.01f, 1.0f }, -0.06f),
		MakeFallback("ガヂィン！！", 0.44f, { 0.9f, 0.62f, 1.0f, 1.0f }, { 0.3f, 0.02f, 0.68f, 1.0f }, 0.08f),
		MakeFallback("ガッシャァン！！", 0.42f, { 1.0f, 0.88f, 0.3f, 1.0f }, { 0.55f, 0.12f, 0.01f, 1.0f }, -0.04f),
		MakeFallback("ヴォンッ！！", 0.44f, { 0.35f, 1.0f, 0.88f, 1.0f }, { 0.02f, 0.3f, 0.72f, 1.0f }, 0.04f),
		MakeFallback("バヂィィン！！", 0.43f, { 1.0f, 0.52f, 0.9f, 1.0f }, { 0.55f, 0.02f, 0.4f, 1.0f }, -0.08f),
	};
	for (std::size_t index = 0; index < names.size(); ++index) {
		ComicTextEffectPreset loaded{};
		if (ComicTextEffectSystem::LoadPreset(names[index], loaded)) {
			// Gimmick captions should accent the action without covering the play field.
			loaded.textScale = (std::min)(loaded.textScale, 0.48f);
			loaded.screenOffset.y = (std::max)(loaded.screenOffset.y, -72.0f);
			presets_[index] = loaded;
		}
	}
	Reset();
	return effects_ != nullptr;
}

void GimmickComicTextSystem::Finalize() noexcept {
	Reset();
	effects_ = nullptr;
}

void GimmickComicTextSystem::Reset() noexcept {
	anchored_.fill({});
	shutterClosed_.fill(false);
	repulsionOccupied_.fill(false);
	snapshotReady_ = false;
}

void GimmickComicTextSystem::Play(Preset preset, const Vector3& position) {
	if (effects_) { effects_->Play(presets_[static_cast<std::size_t>(preset)], position); }
}

void GimmickComicTextSystem::CaptureEvents(
	const MagnetChainSystem& chain, const MagnetStageData& stage) {
	const auto& chainsaw = chain.GetChainsawCutEvent();
	if (chainsaw.occurred) {
		const auto* body = chain.GetPhysicsWorld().GetBody(chainsaw.contactedBody);
		if (body) { Play(Preset::Chainsaw, body->position); }
	}

	const auto& impacts = chain.GetWallImpactEvents();
	for (std::size_t impactIndex = 0; impactIndex < chain.GetWallImpactEventCount(); ++impactIndex) {
		for (std::size_t obstacleIndex = 0; obstacleIndex < stage.obstacleCount; ++obstacleIndex) {
			const auto& obstacle = stage.obstacles[obstacleIndex];
			if (obstacle.obstacleKind != MagnetObstacleKind::PinballBumper) { continue; }
			const float radius = (std::max)(obstacle.size.x, obstacle.size.z) * 0.5f + 0.75f;
			if (DistanceSquaredXZ(impacts[impactIndex].position, obstacle.position) <= radius * radius) {
				Play(Preset::Bumper, impacts[impactIndex].position);
				break;
			}
		}
	}

	const auto& dissolves = chain.GetFurnaceDissolveEvents();
	for (std::size_t index = 0; index < chain.GetFurnaceDissolveEventCount(); ++index) {
		Play(Preset::Furnace, dissolves[index].position);
	}

	const auto& events = chain.GetObstacleEvents();
	for (std::size_t index = 0; index < chain.GetObstacleEventCount(); ++index) {
		if (events[index].type != ObstacleCollisionSystem::EventType::EnterTransferGate) { continue; }
		const auto* destination = FindObstacle(stage, events[index].destinationObstacleId);
		if (destination) { Play(Preset::Transfer, destination->position); }
	}

	for (std::size_t obstacleIndex = 0; obstacleIndex < stage.obstacleCount; ++obstacleIndex) {
		const auto& obstacle = stage.obstacles[obstacleIndex];
		if (obstacle.obstacleKind == MagnetObstacleKind::MagneticAnchor) {
			const auto current = chain.GetAnchoredBody(obstacleIndex);
			if (snapshotReady_ && current.IsValid() && !SameBody(current, anchored_[obstacleIndex])) {
				Play(Preset::Anchor, obstacle.position);
			}
			anchored_[obstacleIndex] = current;
		} else if (obstacle.obstacleKind == MagnetObstacleKind::TimedShutter) {
			const bool closed = chain.IsTimedShutterClosed(obstacleIndex);
			if (snapshotReady_ && closed != shutterClosed_[obstacleIndex]) {
				Play(Preset::Shutter, obstacle.position);
			}
			shutterClosed_[obstacleIndex] = closed;
		} else if (obstacle.obstacleKind == MagnetObstacleKind::RepulsionField) {
			bool occupied = false;
			const float radius = chain.GetRepulsionFieldRadius(obstacle);
			for (std::size_t ballIndex = 0; ballIndex < chain.GetStageBallCount(); ++ballIndex) {
				const auto* body = chain.GetPhysicsWorld().GetBody(chain.GetStageBalls()[ballIndex]);
				if (body && body->active &&
					DistanceSquaredXZ(body->position, obstacle.position) <= radius * radius) {
					occupied = true;
					break;
				}
			}
			if (snapshotReady_ && occupied && !repulsionOccupied_[obstacleIndex]) {
				Play(Preset::Repulsion, obstacle.position);
			}
			repulsionOccupied_[obstacleIndex] = occupied;
		}
	}
	snapshotReady_ = true;
}
} // namespace magnet
