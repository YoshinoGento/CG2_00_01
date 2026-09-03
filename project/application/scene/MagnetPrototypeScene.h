#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/MagnetChainSystem.h"
#include "application/magnet/system/MagneticImpactFeedbackSystem.h"
#include "application/magnet/ui/MagnetPrototypeWindow.h"
#include "application/scene/BaseScene.h"
#include "2d/Sprite.h"

#include <array>
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
	void ProcessStageEditorRequest(
		const magnet::MagnetPrototypeUiRequest& request);
	void SetEditorMode(magnet::MagnetEditorMode mode);
	[[nodiscard]] Vector3 ResolveEditorFocusPosition() const noexcept;
	[[nodiscard]] Vector3 CalculatePlayCameraPosition() noexcept;
	void DrawStageObjects() const;
	void DrawSelectionHighlight() const;
	void DrawWireBox(
		const Vector3& center,
		const Vector3& size,
		const Vector4& color) const;
	void DrawBody(physics::BodyHandle handle, const Vector4& color) const;
	void DrawVelocity(physics::BodyHandle handle) const;
	[[nodiscard]] bool InitializeMinimap();
	void UpdateMinimap();
	void DrawMinimap();
	[[nodiscard]] bool InitializeGoalGuides();
	void UpdateGoalGuides();
	void DrawGoalGuides();

	Framework* framework_ = nullptr;
	std::unique_ptr<Camera> camera_;
	magnet::MagnetStageSystem magnetStageSystem_;
	magnet::MagnetChainSystem magnetChainSystem_;
	magnet::MagneticImpactFeedbackSystem magneticImpactFeedbackSystem_;
	magnet::MagnetPrototypeWindow prototypeWindow_;
	magnet::MagnetChainSystem::PlayerCommand pendingCommand_{};
	bool resetRequested_ = false;
	bool prototypeReady_ = false;
	bool showGrid_ = true;
	bool showVelocity_ = true;
	bool cameraFollow_ = true;
	magnet::MagnetEditorMode editorMode_ = magnet::MagnetEditorMode::Play;
	magnet::MagnetStageObjectType selectedObjectType_ =
		magnet::MagnetStageObjectType::None;
	uint32_t selectedObjectId_ = 0;
	bool releaseOverviewActive_ = false;
	std::unique_ptr<Sprite> minimapBorderSprite_;
	std::unique_ptr<Sprite> minimapBackgroundSprite_;
	std::unique_ptr<Sprite> minimapHorizontalGuideSprite_;
	std::unique_ptr<Sprite> minimapVerticalGuideSprite_;
	std::unique_ptr<Sprite> minimapPlayerSprite_;
	std::array<std::unique_ptr<Sprite>, magnet::MagnetChainSystem::kStageBallCapacity>
		minimapMagnetSprites_{};
	std::size_t minimapMagnetCount_ = 0;
	bool minimapReady_ = false;
	std::array<std::array<std::unique_ptr<Sprite>, 2>,
		magnet::MagnetStageData::kMaximumGoalCount> goalGuideSprites_{};
	std::size_t goalGuideCount_ = 0;
	bool goalGuidesReady_ = false;
};
