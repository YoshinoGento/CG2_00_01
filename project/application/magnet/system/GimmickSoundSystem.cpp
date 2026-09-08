#include "application/magnet/system/GimmickSoundSystem.h"
#include "application/magnet/system/MagnetChainSystem.h"

#include <algorithm>
#include <string>

namespace {
constexpr char kSoundRoot[] = "Resources/audio/magnet/gimmicks/";

bool SameBody(physics::BodyHandle left, physics::BodyHandle right) noexcept {
	return left.index == right.index && left.generation == right.generation;
}

float DistanceSquaredXZ(const Vector3& left, const Vector3& right) noexcept {
	const float x = left.x - right.x;
	const float z = left.z - right.z;
	return x * x + z * z;
}
} // namespace

namespace magnet {
bool GimmickSoundSystem::Initialize(Audio* audio) {
	Finalize();
	const auto path = [](const char* name) { return std::string(kSoundRoot) + name; };
	bool ready = true;
	ready &= bumper_.Initialize(audio, path("pinball_bumper.mp3").c_str(), { 0.72f, 1.0f });
	ready &= furnace_.Initialize(audio, path("furnace_dissolve.mp3").c_str(), { 0.68f, 1.0f });
	ready &= anchor_.Initialize(audio, path("magnetic_anchor.mp3").c_str(), { 0.55f, 1.05f });
	ready &= shutter_.Initialize(audio, path("timed_shutter.mp3").c_str(), { 0.58f, 1.0f });
	ready &= transfer_.Initialize(audio, path("transfer_gate.mp3").c_str(), { 0.62f, 1.0f });
	ready &= repulsion_.Initialize(audio, path("repulsion_field.mp3").c_str(), { 0.52f, 1.10f });
	Reset();
	return ready;
}

void GimmickSoundSystem::Finalize() {
	bumper_.Finalize();
	furnace_.Finalize();
	anchor_.Finalize();
	shutter_.Finalize();
	transfer_.Finalize();
	repulsion_.Finalize();
	Reset();
}

void GimmickSoundSystem::Reset() noexcept {
	anchored_.fill({});
	shutterClosed_.fill(false);
	repulsionOccupied_.fill(false);
	snapshotReady_ = false;
}

void GimmickSoundSystem::Update(
	const MagnetChainSystem& chain,
	const MagnetStageData& stage) {
	const auto& impacts = chain.GetWallImpactEvents();
	for (std::size_t impactIndex = 0;
		impactIndex < chain.GetWallImpactEventCount(); ++impactIndex) {
		for (std::size_t obstacleIndex = 0;
			obstacleIndex < stage.obstacleCount; ++obstacleIndex) {
			const auto& obstacle = stage.obstacles[obstacleIndex];
			if (obstacle.obstacleKind != MagnetObstacleKind::PinballBumper) { continue; }
			const float radius =
				(std::max)(obstacle.size.x, obstacle.size.z) * 0.5f + 0.75f;
			if (DistanceSquaredXZ(impacts[impactIndex].position, obstacle.position) <=
				radius * radius) {
				bumper_.Play();
				break;
			}
		}
	}

	if (chain.GetFurnaceDissolveEventCount() > 0) { furnace_.Play(); }
	const auto& events = chain.GetObstacleEvents();
	for (std::size_t index = 0; index < chain.GetObstacleEventCount(); ++index) {
		if (events[index].type == ObstacleCollisionSystem::EventType::EnterTransferGate) {
			transfer_.Play();
			break;
		}
	}

	for (std::size_t obstacleIndex = 0;
		obstacleIndex < stage.obstacleCount; ++obstacleIndex) {
		const auto& obstacle = stage.obstacles[obstacleIndex];
		if (obstacle.obstacleKind == MagnetObstacleKind::MagneticAnchor) {
			const auto current = chain.GetAnchoredBody(obstacleIndex);
			if (snapshotReady_ && current.IsValid() &&
				!SameBody(current, anchored_[obstacleIndex])) {
				anchor_.Play();
			}
			anchored_[obstacleIndex] = current;
		} else if (obstacle.obstacleKind == MagnetObstacleKind::TimedShutter) {
			const bool closed = chain.IsTimedShutterClosed(obstacleIndex);
			if (snapshotReady_ && closed != shutterClosed_[obstacleIndex]) {
				shutter_.Play();
			}
			shutterClosed_[obstacleIndex] = closed;
		} else if (obstacle.obstacleKind == MagnetObstacleKind::RepulsionField) {
			bool occupied = false;
			const float radius = chain.GetRepulsionFieldRadius(obstacle);
			for (std::size_t ballIndex = 0;
				ballIndex < chain.GetStageBallCount(); ++ballIndex) {
				const auto* body = chain.GetPhysicsWorld().GetBody(
					chain.GetStageBalls()[ballIndex]);
				if (body && body->active &&
					DistanceSquaredXZ(body->position, obstacle.position) <= radius * radius) {
					occupied = true;
					break;
				}
			}
			if (snapshotReady_ && occupied && !repulsionOccupied_[obstacleIndex]) {
				repulsion_.Play();
			}
			repulsionOccupied_[obstacleIndex] = occupied;
		}
	}
	snapshotReady_ = true;
}
} // namespace magnet
