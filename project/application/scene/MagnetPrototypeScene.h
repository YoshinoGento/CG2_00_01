#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "application/magnet/system/MagnetChainSystem.h"
#include "application/magnet/system/MagneticImpactFeedbackSystem.h"
#include "application/magnet/ui/MagnetPrototypeWindow.h"
#include "application/scene/BaseScene.h"
#include "2d/BitmapFont.h"
#include "2d/Sprite.h"
#include "2d/SpriteText.h"
#include "effect/ComicTextEffect.h"
#ifdef USE_IMGUI
#include "debug/ParticleEffectEditor.h"
#endif

#include <array>
#include <memory>

class Camera;
class Framework;
class Input;
class Object3d;
class Skybox;

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
	[[nodiscard]] bool InitializeBallVisuals();
	[[nodiscard]] bool UpdateBallVisuals(float deltaTime) noexcept;
	void DrawBallVisuals() const;
	[[nodiscard]] bool InitializeMinimap();
	void UpdateMinimap();
	void DrawMinimap();
	[[nodiscard]] bool InitializeGoalGuides();
	void UpdateGoalGuides();
	void DrawGoalGuides();
	[[nodiscard]] bool InitializeGameFlowUi();
	void HandlePauseMenuInput(Input& input);
	void RefreshGameFlowUi();
	void DrawGameFlowUi();
	void CompleteTimedGame();

	Framework* framework_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<Object3d> playerVisual_;
	std::array<std::unique_ptr<Object3d>, magnet::MagnetChainSystem::kStageBallCapacity>
		stageBallVisuals_{};
	std::unique_ptr<Skybox> skybox_;
	magnet::MagnetStageSystem magnetStageSystem_;
	magnet::MagnetChainSystem magnetChainSystem_;
	magnet::MagneticImpactFeedbackSystem magneticImpactFeedbackSystem_;
	std::unique_ptr<ComicTextEffectSystem> comicTextEffects_;
	ComicTextEffectPreset heavyImpactPreset_{};
	magnet::MagnetPrototypeWindow prototypeWindow_;
#ifdef USE_IMGUI
	std::unique_ptr<ParticleEffectEditor> particleEffectEditor_;
#endif
	magnet::MagnetChainSystem::PlayerCommand pendingCommand_{};
	bool resetRequested_ = false;
	bool prototypeReady_ = false;
	bool ballVisualsReady_ = false;
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
	BitmapFont gameFlowFont_;
	std::unique_ptr<Sprite> pauseOverlaySprite_;
	std::unique_ptr<Camera> pauseLabelCamera_;
	std::unique_ptr<Object3d> pauseTitleObject_;
	std::unique_ptr<Object3d> resumeLabelObject_;
	std::unique_ptr<Object3d> restartLabelObject_;
	std::unique_ptr<Object3d> backTitleLabelObject_;
	std::unique_ptr<Object3d> volumeLabelObject_;
	SpriteText timerText_;
	SpriteText pauseTitleText_;
	std::array<SpriteText, 4> pauseMenuTexts_{};
	SpriteText pauseHelpText_;
	float gameElapsedSeconds_ = 0.0f;
	int pauseSelection_ = 0;
	bool paused_ = false;
	bool gameFlowUiReady_ = false;
	bool rankingTransitionRequested_ = false;
	bool rightTriggerWasPressed_ = false;
	bool menuStickUpWasPressed_ = false;
	bool menuStickDownWasPressed_ = false;
	bool menuStickLeftWasPressed_ = false;
	bool menuStickRightWasPressed_ = false;
};
