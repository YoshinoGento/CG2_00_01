#pragma once

#include "base/Framework.h"
#include "FieldManager.h"
#include "math/Matrix.h"

#include <array>
#include <memory>

class EditorShell;
class FloatingTextSystem;
class GameplayEffectManager;
class PostEffectSubmissionDemo;
class PostEffectSubmissionHUD;
class PostEffectSystem;
class SceneFactory;
class Sprite;

/**
 * Game class.
 * Application entry point. It owns the main loop, scene flow, and top-level systems.
 */
class Game : public Framework {
public:
	Game();
	~Game() override;

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	void PlayHarvestEffect(const Vector3& position, int32_t price);
	void PlayDigitalImpactEffect(const Vector3& position);
	void UpdateGameplayEffects(float deltaTime);
	void DrawGameplayEffects();
	void DrawGameplayEffectImGui();

	static Vector2 GetMousePosInViewport() { return mousePosInViewport_; }

private:
	enum class AutoDemoStage {
		Idle,
		InitializeField,
		WaitBeforeTill,
		TillTiles,
		WaitAfterTill,
		WaterTiles,
		WaitAfterWater,
		PlantTiles,
		GrowTiles,
		WaitReady,
		HarvestTiles,
		WaitBeforeDigitalImpact,
		DigitalImpact,
		Finished,
	};

	void ApplyDemoRecordingModeSettings();
	void SetPresentationMode(bool enabled);
	void StartAutoDemoSequence();
	void StopAutoDemoSequence();
	void UpdateAutoDemoSequence(float deltaTime);
#ifdef USE_IMGUI
	void DrawDemoRecordingImGui(class GamePlayScene* playScene);
#endif
	void InitializeRuntimeTextTextures();
	void InitializeGameplayHud();
	void DrawGameplayEffectSprites();
	void DrawGameplayHud(class GamePlayScene* playScene);
	void ShowFieldActionMessage(FieldActionFeedbackType type, const Vector3& worldPosition);
	void DrawHudSprite(Sprite* sprite, const Vector2& position, const Vector2& size, const Vector4& color);

	std::unique_ptr<SceneFactory> sceneFactory_;

	static Vector2 mousePosInViewport_;

	std::unique_ptr<GameplayEffectManager> gameplayEffectManager_;
	std::unique_ptr<FloatingTextSystem> floatingTextSystem_;
	std::unique_ptr<PostEffectSystem> postEffectSystem_;
	std::unique_ptr<PostEffectSubmissionDemo> postEffectSubmissionDemo_;
	std::unique_ptr<PostEffectSubmissionHUD> postEffectSubmissionHud_;
#ifdef USE_IMGUI
	std::unique_ptr<EditorShell> editorShell_;
#endif

	bool demoRecordingMode_ = false;
	bool presentationMode_ = false;
	bool hideDebugUI_ = false;
	bool showGameplayHud_ = false;
	bool autoDemoSequenceActive_ = false;
	AutoDemoStage autoDemoStage_ = AutoDemoStage::Idle;
	float autoDemoTimer_ = 0.0f;
	int autoDemoTileCursor_ = 0;
	static constexpr int kAutoDemoTileCount = 9;

	uint32_t hudWhiteTextureHandle_ = 0;
	uint32_t hudControlsLine1TextureHandle_ = 0;
	uint32_t hudControlsLine2TextureHandle_ = 0;
	uint32_t hudSelectedLabelTextureHandle_ = 0;
	uint32_t hudGrowthLabelTextureHandle_ = 0;
	uint32_t hudMoistureLabelTextureHandle_ = 0;
	std::array<uint32_t, 5> hudStateTextureHandles_{};
	std::array<uint32_t, 5> hudNextTextureHandles_{};
	std::array<uint32_t, 101> hudPercentTextureHandles_{};

	std::unique_ptr<Sprite> hudControlsPanelSprite_;
	std::unique_ptr<Sprite> hudControlsLine1Sprite_;
	std::unique_ptr<Sprite> hudControlsLine2Sprite_;
	std::unique_ptr<Sprite> hudStatusPanelSprite_;
	std::unique_ptr<Sprite> hudSelectedLabelSprite_;
	std::unique_ptr<Sprite> hudStateValueSprite_;
	std::unique_ptr<Sprite> hudGrowthLabelSprite_;
	std::unique_ptr<Sprite> hudMoistureLabelSprite_;
	std::unique_ptr<Sprite> hudGrowthBarBackgroundSprite_;
	std::unique_ptr<Sprite> hudGrowthBarFillSprite_;
	std::unique_ptr<Sprite> hudMoistureBarBackgroundSprite_;
	std::unique_ptr<Sprite> hudMoistureBarFillSprite_;
	std::unique_ptr<Sprite> hudStateAccentSprite_;
	std::unique_ptr<Sprite> hudNextActionSprite_;
	std::unique_ptr<Sprite> hudGrowthPercentSprite_;
	std::unique_ptr<Sprite> hudMoisturePercentSprite_;

	Vector2 gameViewportImageTopLeft_ = { 0.0f, 0.0f };
	Vector2 gameViewportImageSize_ = { 1280.0f, 720.0f };
};
