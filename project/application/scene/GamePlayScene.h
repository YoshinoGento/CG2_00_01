#pragma once

#include "application/level/LevelGameplaySystem.h"
#include "BaseScene.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <string>
#include <cstdint>
#include "math/Matrix.h"
#include "3d/Skybox.h"
#include "2d/TextureManager.h"
#include "3d/SkeletonDebugger.h"
#include "effect/ParticleManager.h"
#include "time/SnapshotTimeline.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDateSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/ui/FarmHUD.h"
#ifdef USE_IMGUI
#include "debug/DebugEditorWindow.h"
#include "farm/debug/FarmDebugEditorWindow.h"
#endif

class Framework;
class Sprite;
class Object3d;
class Camera;
class Model;

namespace level {
struct LevelData;
}

class GamePlayScene : public BaseScene {
	friend class Game;
public:
	enum class GPUParticleDebugMode {
		Off,
		Agriculture,
		Interaction,
	};

	enum class AgricultureParticleType {
		DirtDust,
		WaterSplash,
		HarvestSparkle,
		PollenSpore,
		BugSwarm,
	};

	GamePlayScene();
	~GamePlayScene() override;

	void Initialize() override;
	void Finalize() override;
	void PrepareFixedUpdate() override;
	void FixedUpdate(float fixedDeltaTime) override;
	void Update() override;
	void Draw() override;
	void CreateSphere(float radius);
	// Cylinderメッシュの再生成（パラメータ変更時に呼ぶ）
	void RebuildCylinder();
	// アニメーションモデルの動的切り替え（ImGuiからの呼び出し用）
	void ChangeAnimationModel(int index);

private:
	enum class PlayerAnimationMode;
	struct GameplaySnapshot;
	void AddLog(const std::string& message);
	void UpdateSceneDeltaTime();
	void HandleKeyboardMovement();
	void HandleCameraInput(float deltaTime, bool suppressArrowKeys);
	void ClampCameraPitch();
	void ResetCamera();
	void SetUsePlayerCamera(bool usePlayerCamera);
	void ToggleCameraMode();
	void UpdatePlayerCamera();
	void SyncGPUParticleDebugModeChange();
	void SetGPUParticleDebugMode(GPUParticleDebugMode mode);
	void HandleGPUParticleDebugModeInput();
	void HandleAgricultureParticleInput();
	void HandleInteractionParticleInput();
	void EmitAgricultureParticle(AgricultureParticleType type);
	uint32_t CalculateInteractionParticleCount() const;
	bool TryGetInteractionBrushPosition(Vector3& outBrushPosition) const;
	FarmHUDViewData BuildFarmHUDViewData() const;
	void HandleFarmDateDebugInput();
	void HandleFarmHistoryInput();
	void HandleFarmToolDebugInput();
	void HandleFarmToolActionInput();
	bool HandleFarmGridSelectionInput();
	void InitializeFarmHUD();
	void InitializeSkyboxIfNeeded();
	void LoadSceneLevel();
	void CreateLevelObjectsFromLevel();
	void SyncLevelGameplayPresentation();
	void InitializeTimeline();
	void CaptureTimelineSnapshot(GameplaySnapshot& output) const;
	bool RestoreTimelineSnapshot(const GameplaySnapshot& snapshot);
	void UpdateLevelPlayerVisual();
	void ConfigureLevelPlayerAnimation(std::size_t playerRenderIndex, PlayerAnimationMode mode);
	void CollectLevelRuntimeData();
	void ApplyLevelCamera();
	void DrawLevelDebugGizmos() const;
	void DrawLevelCollisionGizmos() const;
	void DrawLevelBox(const Vector3& center, const Vector3& size, const Vector4& color) const;
	void DrawLevelCameraGizmo(const Vector3& position, const Vector4& color) const;
	Vector3 EvaluateLevelRoutePoint(float normalizedTime) const;
#ifdef USE_IMGUI
	void DrawSceneDebugWindow();
	void SpawnDebugEditorObject(const DebugEditorWindow::SpawnRequest& request);
	Object3d* PickDebugObjectFromViewport(const Vector2& viewportMousePosition) const;
#endif

	// エフェクト発生関数
	void EmitSpark(const Vector3& position);
	void EmitRingEffect(const Vector3& position);
	void EmitCylinderEffect(const Vector3& position);

private:
	struct LevelObjectRuntime {
		std::unique_ptr<Object3d> object;
		std::string name;
		std::string gameplayRole;
		Vector3 basePosition{};
		float animationPhase = 0.0f;
		bool visible = true;
	};
#ifdef USE_IMGUI
	struct DebugSpawnedObjectRuntime {
		std::unique_ptr<Object3d> object;
		std::string name;
		std::string modelPath;
		bool animated = false;
	};
#endif
	struct GameplaySnapshot {
		level::LevelGameplaySystem::Snapshot levelGameplay;
		farm::FarmGrid::Snapshot farmGrid;
		FarmDateSystem::Snapshot farmDate;
		FarmTool farmTool = FarmTool::Hoe;
		float levelRouteTimer = 0.0f;
		float playerAnimationTime = 0.0f;
		float playerAnimationSpeed = 0.0f;
	};

	enum class PlayerAnimationMode {
		Walk,
		Sneak,
	};
	struct PlayerAnimationClip {
		const char* fileName = nullptr;
		float playbackSpeed = 1.0f;
	};
	enum class PlayerAnimationState {
		Idle,
		Walk,
		Sneak,
		Airborne,
	};
	static const PlayerAnimationClip& GetPlayerAnimationClip(PlayerAnimationMode mode);

	Framework* framework_ = nullptr;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Camera> camera_;

	Vector3 cameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 cameraRot_ = { 0.3f, 0.0f, 0.0f };
	Vector3 debugCameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 debugCameraRot_ = { 0.3f, 0.0f, 0.0f };
	Vector3 levelCameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 levelCameraRot_ = { 0.3f, 0.0f, 0.0f };
	float cameraMoveSpeed_ = 5.0f;
	float cameraRotateSpeed_ = 1.5f;
	float sceneDeltaTime_ = 1.0f / 60.0f;

	std::unique_ptr<Model> sphereModel_;
	std::unique_ptr<Object3d> sphereObj_;
	std::unique_ptr<Model> terrainModel_;
	std::unique_ptr<Object3d> terrainObj_;

	std::unique_ptr<Object3d> animObj_;
	std::unique_ptr<level::LevelData> levelData_;
	std::vector<LevelObjectRuntime> levelObjects_;
	level::LevelGameplaySystem levelGameplay_;
	level::LevelGameplaySystem::PlayerCommand pendingPlayerCommand_{};
	SnapshotTimeline<GameplaySnapshot> timeline_;
	GameplaySnapshot timelineScratch_{};
	bool timelineRewindHeld_ = false;
	bool timelineForwardHeld_ = false;
	bool timelineScrubbing_ = false;
	bool timelineStepBackwardRequested_ = false;
	bool timelineStepForwardRequested_ = false;
	Texture2DHandle levelWhiteTextureHandle_{};
	PlayerAnimationMode playerAnimationMode_ = PlayerAnimationMode::Walk;
	PlayerAnimationState playerAnimationState_ = PlayerAnimationState::Idle;
	float playerAnimationSpeed_ = 0.0f;
	bool playerVisualConfigured_ = false;
	bool directionalShadowsEnabled_ = true;
	float directionalShadowStrength_ = 0.82f;
	std::vector<Vector3> levelRoutePoints_;
	float levelRouteTimer_ = 0.0f;

	// エフェクト用モデル
	std::unique_ptr<Model> ringModel_;
	std::unique_ptr<Model> cylinderModel_;

	std::unique_ptr<Skybox> skybox_;
	bool skyboxEnabled_ = false;
	bool skyboxEnvironmentEnabled_ = false;
	int skyboxSelection_ = 0;
	std::vector<Texture2DHandle> textureHandles_;
	Texture2DHandle ringTexHandle_{};

	std::unique_ptr<SkeletonDebugger> skeletonDebugger_;
	farm::FarmGrid farmGrid_;
	FarmDateSystem farmDateSystem_;
	FarmToolSystem farmToolSystem_;
	FarmToolActionSystem farmToolActionSystem_;
	FarmHUD farmHud_;
#ifdef USE_IMGUI
	farm::FarmDebugEditorWindow farmDebugEditorWindow_;
	Object3d* debugSelectedObject_ = nullptr;
	std::vector<DebugSpawnedObjectRuntime> debugSpawnedObjects_;
	uint32_t debugSpawnCounter_ = 0;
	bool showSceneDebugUi_ = true;
	bool showFarmDebugUi_ = true;
#endif
	bool farmHudInitialized_ = false;

	bool showTerrain_ = true;
	bool showSphere_ = true;
	bool showPlane_ = true;
	bool showSprite_ = true;
	bool showParticles_ = true;
	bool showAnimModel_ = true;
	bool showLevelObjects_ = true;
	bool showLevelGizmos_ = true;
	bool showLevelCollisionGizmos_ = true;
	bool usePlayerCamera_ = true;
	bool hasLevelCamera_ = false;
	bool showSkeleton_ = false;

	Vector2 spritePos_ = { 640.0f, 360.0f };
	Vector3 objectPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 objectRot_ = { 0.0f, 0.0f, 0.0f };
	Vector3 spherePos_ = { 0.0f, 0.0f, 10.0f };
	float sphereRadius_ = 1.0f;

	int selectedTarget_ = 4;
	int activeParticleType_ = 0; // これを Game.cpp と同期
	int currentAnimModelIdx_ = 1; // 現在のアニメーションモデルのインデックス（0: AnimatedCube, 1: simpleSkin, 2: human/walk, 3: human/sneakWalk）
	int cullMode_ = 2;
	int modelPriority_ = 0;

	GPUParticleDebugMode gpuParticleDebugMode_ = GPUParticleDebugMode::Off;
	GPUParticleDebugMode previousGPUParticleDebugMode_ = GPUParticleDebugMode::Off;
	Vector3 agricultureEmitPosition_ = { 0.0f, 1.5f, 8.0f };
	float agricultureParticleSize_ = 0.25f;
	int agricultureParticleCount_ = 128;
	bool agricultureShowKeyGuide_ = true;
	int interactionGridCount_ = 8;
	float interactionParticleSize_ = 0.03f;
	float interactionBrushRadius_ = 1.5f;
	float interactionBrushStrength_ = 1.0f;
	float interactionDamping_ = 0.95f;
	Vector3 interactionGridCenter_ = { 0.0f, 0.0f, 8.0f };
	Vector3 interactionBrushPosition_ = { 0.0f, 0.0f, 8.0f };
	uint32_t interactionParticleCount_ = 512;
	bool interactionResetRequested_ = true;
	InteractionBrushOperation interactionBrushOperation_ = InteractionBrushOperation::None;
	Vector2 viewportImageTopLeft_ = { 0.0f, 0.0f };
	Vector2 viewportImageSize_ = { 0.0f, 0.0f };
	Vector2 viewportMousePosition_ = { 0.0f, 0.0f };
	bool viewportHovered_ = false;

	// Cylinderパラメータ（ImGuiで操作可能）
	float cylTopRadius_ = 1.0f;       // 上面の半径
	float cylBottomRadius_ = 1.0f;    // 下面の半径
	float cylHeight_ = 2.0f;          // 高さ
	int cylSegments_ = 32;            // 円周方向の分割数
	int cylVertDivisions_ = 4;        // 高さ方向の分割数

	// ライト設定 (GamePlayScene.cpp で使用しているもの)
	Vector3 lightDirection_ = { 0.0f, -1.0f, 1.0f };
	Vector3 lightColor_ = { 1.0f, 1.0f, 1.0f };
	float lightIntensity_ = 1.0f;
	Vector3 spotLightColor_ = { 1.0f, 1.0f, 1.0f };
	Vector3 spotLightPos_ = { 0.0f, 4.0f, 10.0f };
	float spotLightIntensity_ = 0.0f;
	Vector3 spotLightDir_ = { 0.0f, -1.0f, 0.0f };
	float spotLightDistance_ = 15.0f;
	float spotLightDecay_ = 1.0f;
	float spotLightAngle_ = 0.5f;
	float spotLightFalloff_ = 0.1f;

	std::vector<std::string> debugLogs_;
};
