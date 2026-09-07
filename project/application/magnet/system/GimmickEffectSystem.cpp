#include "application/magnet/system/GimmickEffectSystem.h"

#include "3d/LineDrawer.h"
#include "application/magnet/system/MagnetChainSystem.h"
#include "effect/ParticleManager.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kRingSegments = 28;

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
} // namespace

namespace magnet {
bool GimmickEffectSystem::Initialize(ParticleManager* particleManager) noexcept {
	Finalize();
	particleManager_ = particleManager;
	Reset();
	return particleManager_ != nullptr;
}

void GimmickEffectSystem::Finalize() noexcept {
	Reset();
	particleManager_ = nullptr;
}

void GimmickEffectSystem::Reset() noexcept {
	lightning_.Clear();
	ringCount_ = 0;
	anchored_.fill({});
	shutterClosed_.fill(false);
	repulsionOccupied_.fill(false);
	snapshotReady_ = false;
	effectSeed_ = 1;
}

void GimmickEffectSystem::AddRing(const Vector3& position, const Vector4& color,
	float startRadius, float endRadius, float duration, RingStyle style) {
	if (ringCount_ >= rings_.size()) {
		std::move(rings_.begin() + 1, rings_.begin() + ringCount_, rings_.begin());
		--ringCount_;
	}
	rings_[ringCount_++] = {
		position, color, startRadius, endRadius, duration, 0.0f, style };
}

void GimmickEffectSystem::EmitBurst(const Vector3& position, const Vector4& color,
	uint32_t count, float radius, float speed, float lifeTime,
	const Vector3& direction, float spread, const Vector3& scale) {
	if (!particleManager_) { return; }
	GPUParticleEmitSettings settings{};
	settings.translate = position;
	settings.radius = radius;
	settings.color = color;
	settings.scale = scale;
	settings.lifeTime = lifeTime;
	settings.baseVelocity = {};
	settings.speed = speed;
	settings.count = count;
	settings.emit = 1;
	settings.preset = 0;
	settings.extendedSettings = 1;
	settings.direction = direction;
	settings.directionSpread = spread;
	settings.acceleration = { 0.0f, -0.4f, 0.0f };
	settings.drag = 0.8f;
	settings.endScale = scale * 0.12f;
	settings.endAlpha = 0.0f;
	settings.colorVariance = { 0.12f, 0.12f, 0.12f, 0.0f };
	settings.lifeTimeVariance = lifeTime * 0.25f;
	settings.speedVariance = speed * 0.3f;
	settings.scaleVariance = scale.x * 0.35f;
	settings.innerRadius = 0.0f;
	settings.shape = 0;
	settings.randomSeed = effectSeed_++;
	settings.fadeMode = 1;
	particleManager_->RequestGPUParticleEmit(settings);
}

void GimmickEffectSystem::PlayBumper(const Vector3& position, float strength) {
	const float size = std::clamp(strength * 0.07f, 0.8f, 2.2f);
	AddRing(position + Vector3{ 0.0f, 0.35f, 0.0f }, { 0.2f, 0.9f, 1.0f, 1.0f },
		0.18f, size, 0.24f, RingStyle::Horizontal);
	EmitBurst(position + Vector3{ 0.0f, 0.35f, 0.0f }, { 0.55f, 0.95f, 1.0f, 1.0f },
		32, 0.16f, 3.6f, 0.38f, { 0.0f, 1.0f, 0.0f }, 3.14159265f,
		{ 0.13f, 0.13f, 0.13f });
}

void GimmickEffectSystem::PlayFurnace(const Vector3& position) {
	AddRing(position, { 1.0f, 0.22f, 0.02f, 0.95f }, 0.15f, 1.35f, 0.38f,
		RingStyle::Horizontal);
	EmitBurst(position, { 1.0f, 0.34f, 0.04f, 1.0f }, 48, 0.35f, 2.7f, 0.72f,
		{ 0.0f, 1.0f, 0.0f }, 1.15f, { 0.18f, 0.18f, 0.18f });
}

void GimmickEffectSystem::PlayAnchor(const Vector3& position, uint32_t seed) {
	AddRing(position + Vector3{ 0.0f, 0.4f, 0.0f }, { 0.62f, 0.2f, 1.0f, 0.95f },
		1.2f, 0.15f, 0.34f, RingStyle::VerticalCross);
	LightningEffectSettings settings{};
	settings.direction = { 1.0f, 0.0f, 0.0f };
	settings.length = 1.25f;
	settings.width = 0.018f;
	settings.jaggedness = 0.12f;
	settings.segments = 5;
	settings.boltCount = 5;
	settings.branches = 0;
	settings.duration = 0.22f;
	settings.glowColor = { 0.45f, 0.08f, 1.0f, 0.62f };
	settings.coreColor = { 0.92f, 0.75f, 1.0f, 1.0f };
	settings.randomSeed = seed;
	lightning_.Play(settings, position + Vector3{ 0.0f, 0.4f, 0.0f });
}

void GimmickEffectSystem::PlayShutter(const Vector3& position, bool closed) {
	const Vector4 color = closed
		? Vector4{ 1.0f, 0.35f, 0.08f, 1.0f }
		: Vector4{ 1.0f, 0.88f, 0.22f, 1.0f };
	AddRing(position + Vector3{ 0.0f, 0.5f, 0.0f }, color,
		closed ? 1.2f : 0.2f, closed ? 0.2f : 1.2f, 0.3f,
		RingStyle::VerticalCross);
	EmitBurst(position + Vector3{ 0.0f, 0.35f, 0.0f }, color, 24, 0.65f, 2.0f,
		0.32f, { 0.0f, 1.0f, 0.0f }, 2.4f, { 0.1f, 0.1f, 0.1f });
}

void GimmickEffectSystem::PlayTransfer(const Vector3& source, const Vector3& destination) {
	const Vector4 sourceColor{ 0.1f, 0.7f, 1.0f, 1.0f };
	const Vector4 destinationColor{ 0.1f, 1.0f, 0.72f, 1.0f };
	AddRing(source + Vector3{ 0.0f, 0.4f, 0.0f }, sourceColor, 1.25f, 0.1f, 0.34f,
		RingStyle::VerticalCross);
	AddRing(destination + Vector3{ 0.0f, 0.4f, 0.0f }, destinationColor, 0.1f, 1.4f, 0.34f,
		RingStyle::VerticalCross);
	EmitBurst(destination + Vector3{ 0.0f, 0.4f, 0.0f }, destinationColor, 40,
		0.25f, 2.5f, 0.55f, { 0.0f, 1.0f, 0.0f }, 2.2f,
		{ 0.12f, 0.12f, 0.12f });
}

void GimmickEffectSystem::PlayRepulsion(const Vector3& position, float radius, uint32_t seed) {
	const float endRadius = std::clamp(radius, 0.8f, 5.0f);
	AddRing(position + Vector3{ 0.0f, 0.25f, 0.0f }, { 1.0f, 0.18f, 0.72f, 1.0f },
		0.12f, endRadius, 0.42f, RingStyle::Horizontal);
	LightningEffectSettings settings{};
	settings.length = endRadius * 0.7f;
	settings.width = 0.016f;
	settings.jaggedness = 0.1f;
	settings.segments = 5;
	settings.boltCount = 8;
	settings.spreadDegrees = 360.0f;
	settings.branches = 0;
	settings.duration = 0.18f;
	settings.glowColor = { 1.0f, 0.05f, 0.5f, 0.5f };
	settings.coreColor = { 1.0f, 0.72f, 0.92f, 1.0f };
	settings.randomSeed = seed;
	lightning_.Play(settings, position + Vector3{ 0.0f, 0.25f, 0.0f });
}

void GimmickEffectSystem::CaptureEvents(
	const MagnetChainSystem& chain, const MagnetStageData& stage) {
	const auto& impacts = chain.GetWallImpactEvents();
	for (std::size_t impactIndex = 0; impactIndex < chain.GetWallImpactEventCount(); ++impactIndex) {
		for (std::size_t obstacleIndex = 0; obstacleIndex < stage.obstacleCount; ++obstacleIndex) {
			const auto& obstacle = stage.obstacles[obstacleIndex];
			if (obstacle.obstacleKind != MagnetObstacleKind::PinballBumper) { continue; }
			const float radius = (std::max)(obstacle.size.x, obstacle.size.z) * 0.5f + 0.75f;
			if (DistanceSquaredXZ(impacts[impactIndex].position, obstacle.position) <= radius * radius) {
				PlayBumper(impacts[impactIndex].position, impacts[impactIndex].relativeSpeed);
				break;
			}
		}
	}

	const auto& dissolves = chain.GetFurnaceDissolveEvents();
	for (std::size_t index = 0; index < chain.GetFurnaceDissolveEventCount(); ++index) {
		PlayFurnace(dissolves[index].position);
	}

	const auto& events = chain.GetObstacleEvents();
	for (std::size_t index = 0; index < chain.GetObstacleEventCount(); ++index) {
		if (events[index].type != ObstacleCollisionSystem::EventType::EnterTransferGate) { continue; }
		const auto* source = FindObstacle(stage, events[index].obstacleId);
		const auto* destination = FindObstacle(stage, events[index].destinationObstacleId);
		if (source && destination) { PlayTransfer(source->position, destination->position); }
	}

	for (std::size_t obstacleIndex = 0; obstacleIndex < stage.obstacleCount; ++obstacleIndex) {
		const auto& obstacle = stage.obstacles[obstacleIndex];
		if (obstacle.obstacleKind == MagnetObstacleKind::MagneticAnchor) {
			const auto current = chain.GetAnchoredBody(obstacleIndex);
			if (snapshotReady_ && current.IsValid() && !SameBody(current, anchored_[obstacleIndex])) {
				PlayAnchor(obstacle.position, effectSeed_++);
			}
			anchored_[obstacleIndex] = current;
		} else if (obstacle.obstacleKind == MagnetObstacleKind::TimedShutter) {
			const bool closed = chain.IsTimedShutterClosed(obstacleIndex);
			if (snapshotReady_ && closed != shutterClosed_[obstacleIndex]) {
				PlayShutter(obstacle.position, closed);
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
				PlayRepulsion(obstacle.position, radius, effectSeed_++);
			}
			repulsionOccupied_[obstacleIndex] = occupied;
		}
	}
	snapshotReady_ = true;
}

void GimmickEffectSystem::Update(float deltaTime) {
	if (!std::isfinite(deltaTime)) { return; }
	deltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
	lightning_.Update(deltaTime);
	for (std::size_t index = 0; index < ringCount_;) {
		rings_[index].elapsed += deltaTime;
		if (rings_[index].elapsed >= rings_[index].duration) {
			rings_[index] = rings_[ringCount_ - 1];
			--ringCount_;
		} else {
			++index;
		}
	}
}

void GimmickEffectSystem::Draw(LineDrawer& lineDrawer) const {
	lightning_.Draw(lineDrawer);
	for (std::size_t ringIndex = 0; ringIndex < ringCount_; ++ringIndex) {
		const auto& ring = rings_[ringIndex];
		const float progress = std::clamp(ring.elapsed / ring.duration, 0.0f, 1.0f);
		const float radius = ring.startRadius + (ring.endRadius - ring.startRadius) * progress;
		Vector4 color = ring.color;
		color.w *= 1.0f - progress;
		const auto drawRing = [&](int plane) {
			for (int segment = 0; segment < kRingSegments; ++segment) {
				const float a = kTwoPi * static_cast<float>(segment) / kRingSegments;
				const float b = kTwoPi * static_cast<float>(segment + 1) / kRingSegments;
				Vector3 first{};
				Vector3 second{};
				if (plane == 0) {
					first = { std::cos(a) * radius, 0.0f, std::sin(a) * radius };
					second = { std::cos(b) * radius, 0.0f, std::sin(b) * radius };
				} else if (plane == 1) {
					first = { std::cos(a) * radius, std::sin(a) * radius, 0.0f };
					second = { std::cos(b) * radius, std::sin(b) * radius, 0.0f };
				} else {
					first = { 0.0f, std::sin(a) * radius, std::cos(a) * radius };
					second = { 0.0f, std::sin(b) * radius, std::cos(b) * radius };
				}
				lineDrawer.DrawLine(ring.position + first, ring.position + second, color);
			}
		};
		drawRing(0);
		if (ring.style == RingStyle::VerticalCross) {
			drawRing(1);
			drawRing(2);
		}
	}
}
} // namespace magnet
