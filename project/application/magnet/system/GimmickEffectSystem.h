#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "effect/ProceduralCombatEffect.h"
#include "physics/PhysicsWorld.h"

#include <array>
#include <cstddef>

class LineDrawer;
class ParticleManager;

namespace magnet {
class MagnetChainSystem;

// Converts obstacle contacts and state changes into short-lived gameplay effects.
class GimmickEffectSystem final {
public:
	[[nodiscard]] bool Initialize(ParticleManager* particleManager) noexcept;
	void Finalize() noexcept;
	void Reset() noexcept;
	void CaptureEvents(const MagnetChainSystem& chain, const MagnetStageData& stage);
	void Update(float deltaTime);
	void Draw(LineDrawer& lineDrawer) const;

private:
	enum class RingStyle : uint8_t { Horizontal, VerticalCross };
	struct ActiveRing {
		Vector3 position{};
		Vector4 color{};
		float startRadius = 0.0f;
		float endRadius = 1.0f;
		float duration = 0.25f;
		float elapsed = 0.0f;
		RingStyle style = RingStyle::Horizontal;
	};

	static constexpr std::size_t kMaximumRings = 48;
	void AddRing(const Vector3& position, const Vector4& color,
		float startRadius, float endRadius, float duration, RingStyle style);
	void EmitBurst(const Vector3& position, const Vector4& color,
		uint32_t count, float radius, float speed, float lifeTime,
		const Vector3& direction, float spread, const Vector3& scale);
	void PlayBumper(const Vector3& position, float strength);
	void PlayFurnace(const Vector3& position);
	void PlayAnchor(const Vector3& position, uint32_t seed);
	void PlayShutter(const Vector3& position, bool closed);
	void PlayTransfer(const Vector3& source, const Vector3& destination);
	void PlayRepulsion(const Vector3& position, float radius, uint32_t seed);

	ParticleManager* particleManager_ = nullptr;
	LightningEffect lightning_;
	std::array<ActiveRing, kMaximumRings> rings_{};
	std::size_t ringCount_ = 0;
	std::array<physics::BodyHandle, MagnetStageData::kMaximumObstacleCount> anchored_{};
	std::array<bool, MagnetStageData::kMaximumObstacleCount> shutterClosed_{};
	std::array<bool, MagnetStageData::kMaximumObstacleCount> repulsionOccupied_{};
	bool snapshotReady_ = false;
	uint32_t effectSeed_ = 1;
};
} // namespace magnet
