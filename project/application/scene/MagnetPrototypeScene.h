#pragma once

#include "application/magnet/system/MagnetChainSystem.h"
#include "application/magnet/ui/MagnetPrototypeWindow.h"
#include "application/scene/BaseScene.h"

#include <memory>

class Camera;
class Framework;

// Isolated visual test for fixed-step magnet-chain behavior.
class MagnetPrototypeScene final : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void PrepareFixedUpdate() override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Update() override;
	void Draw() override;
	bool UsesEditorShell() const noexcept override { return false; }
	void DrawEditorUi(const SceneEditorContext& context) override;

private:
	void DrawBody(physics::BodyHandle handle, const Vector4& color) const;
	void DrawVelocity(physics::BodyHandle handle) const;

	Framework* framework_ = nullptr;
	std::unique_ptr<Camera> camera_;
	magnet::MagnetChainSystem magnetChainSystem_;
	magnet::MagnetPrototypeWindow prototypeWindow_;
	magnet::MagnetChainSystem::PlayerCommand pendingCommand_{};
	bool resetRequested_ = false;
	bool prototypeReady_ = false;
	bool showGrid_ = true;
	bool showVelocity_ = true;
	bool cameraFollow_ = true;
};
