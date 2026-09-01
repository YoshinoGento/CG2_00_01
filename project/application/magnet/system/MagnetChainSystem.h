#pragma once

#include "physics/PhysicsWorld.h"

#include <array>
#include <cstddef>

namespace magnet {

// Owns the prototype player command and the left/right chain topology.
// Rendering receives read-only body state from PhysicsWorld.
class MagnetChainSystem final {
public:
	static constexpr std::size_t kLinksPerSide = 4;
	static constexpr std::size_t kTestBallCapacity = 24;

	struct EmitterSettings {
		bool autoEmit = false;
		float intervalSeconds = 0.35f;
		float launchSpeed = 8.0f;
	};

	struct PlayerCommand {
		Vector3 moveDirection{};
		bool emergencyStop = false;
		bool emitOne = false;
	};

	[[nodiscard]] bool Initialize();
	[[nodiscard]] bool Reset();
	void SetPlayerCommand(const PlayerCommand& command) noexcept { command_ = command; }
	void SetEmitterSettings(const EmitterSettings& settings) noexcept;
	[[nodiscard]] bool FixedUpdate(float fixedDeltaTime) noexcept;

	[[nodiscard]] const physics::PhysicsWorld& GetPhysicsWorld() const noexcept { return physicsWorld_; }
	[[nodiscard]] physics::BodyHandle GetPlayerBody() const noexcept { return playerBody_; }
	[[nodiscard]] const std::array<physics::BodyHandle, kLinksPerSide>& GetLeftChain() const noexcept { return leftChain_; }
	[[nodiscard]] const std::array<physics::BodyHandle, kLinksPerSide>& GetRightChain() const noexcept { return rightChain_; }
	[[nodiscard]] const std::array<physics::BodyHandle, kTestBallCapacity>& GetTestBalls() const noexcept { return testBalls_; }
	[[nodiscard]] std::size_t GetActiveTestBallCount() const noexcept;
	[[nodiscard]] float GetMaximumConstraintError() const noexcept;
	[[nodiscard]] bool IsHealthy() const noexcept { return healthy_; }

private:
	[[nodiscard]] bool CreateChain(
		float sideSign,
		std::array<physics::BodyHandle, kLinksPerSide>& outputChain);
	[[nodiscard]] bool CreateTestBallPool();
	[[nodiscard]] bool EmitTestBall() noexcept;
	void DeactivateDistantTestBalls() noexcept;

	physics::PhysicsWorld physicsWorld_;
	physics::BodyHandle playerBody_{};
	std::array<physics::BodyHandle, kLinksPerSide> leftChain_{};
	std::array<physics::BodyHandle, kLinksPerSide> rightChain_{};
	std::array<physics::BodyHandle, kTestBallCapacity> testBalls_{};
	PlayerCommand command_{};
	EmitterSettings emitterSettings_{};
	Vector3 playerVelocity_{};
	float emitterTimer_ = 0.0f;
	std::size_t nextTestBallIndex_ = 0;
	uint32_t emittedBallSequence_ = 0;
	bool healthy_ = false;
};

} // namespace magnet
