#pragma once
#include "base/Framework.h"
#include "math/Matrix.h"	
#include <memory>

class SceneFactory;
class EditorShell;
class PostEffectSystem;
class PostEffectSubmissionDemo;
class PostEffectSubmissionHUD;

/**
 * Gameクラス
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

	// Mouse position converted to the current Game Viewport coordinate space.
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
	void DrawDemoRecordingImGui(class GamePlayScene* playScene);
	void InitializeRuntimeTextTextures();
	void InitializeGameplayHud();
	void DrawGameplayEffectSprites();
	void DrawGameplayHud(class GamePlayScene* playScene);
	void ShowFieldActionMessage(FieldActionFeedbackType type, const Vector3& worldPosition);

	std::unique_ptr<SceneFactory> sceneFactory_;

	// 計算・補正されたマウス座標を保持
	static Vector2 mousePosInViewport_;

	std::unique_ptr<PostEffectSystem> postEffectSystem_;
	std::unique_ptr<PostEffectSubmissionDemo> postEffectSubmissionDemo_;
	std::unique_ptr<PostEffectSubmissionHUD> postEffectSubmissionHud_;
#ifdef USE_IMGUI
	std::unique_ptr<EditorShell> editorShell_;
#endif
};
