#pragma once
#include "base/Framework.h"
#include "math/Matrix.h"	
#include <memory>
#include "effect/PostProcess.h"

class SceneFactory;
class SkinningDebugWindow;

/**
 * Gameクラス
 * プロジェクトのメイン管理者。エディタUI（DockSpace）と
 * ゲーム画面の表示・座標補正を担当します。
 */
class Game : public Framework {
public:
	Game();
	~Game() override;

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	// 各シーンから「補正後のマウス座標」を取得するための関数
	static Vector2 GetMousePosInViewport() { return mousePosInViewport_; }

private:
	std::unique_ptr<SceneFactory> sceneFactory_;

	// RenderTexture / PostEffect result SRV indices.
	uint32_t renderTextureSrvIndex_ = 0;
	uint32_t postEffectResultSrvIndex_ = 0;
	uint32_t depthBufferSrvIndex_ = 0;
	uint32_t normalTextureSrvIndex_ = 0;
	int fullscreenPostEffectIndex_ = 0;
	float fullscreenGrayscaleIntensity_ = 1.0f;
	float fullscreenSepiaIntensity_ = 1.0f;
	float fullscreenBlurStrength_ = 4.0f;
	float fullscreenBloomThreshold_ = 0.65f;
	float fullscreenBloomIntensity_ = 1.5f;
	float fullscreenBloomRadius_ = 8.0f;
	float fullscreenBloomSoftKnee_ = 0.2f;
	Vector2 fullscreenRadialBlurCenter_ = { 0.5f, 0.5f };
	float fullscreenRadialBlurWidth_ = 0.01f;
	float fullscreenRadialBlurIntensity_ = 1.0f;
	int fullscreenRadialBlurSampleCount_ = 10;
	float fullscreenOutlineThreshold_ = 0.15f;
	float fullscreenOutlineIntensity_ = 1.0f;
	float fullscreenOutlineThickness_ = 1.0f;
	float fullscreenDepthOutlineThreshold_ = 0.001f;
	float fullscreenDepthOutlineIntensity_ = 1.0f;
	float fullscreenDepthOutlineThickness_ = 1.0f;
	bool fullscreenDepthOutlineLinearize_ = true;
	float fullscreenNormalOutlineThreshold_ = 0.25f;
	float fullscreenNormalOutlineIntensity_ = 1.0f;
	float fullscreenNormalOutlineThickness_ = 1.0f;
	bool fullscreenVignetteEnabled_ = false;
	float fullscreenVignetteScale_ = 16.0f;
	float fullscreenVignettePower_ = 0.8f;
	float fullscreenVignetteIntensity_ = 1.0f;

	// 計算・補正されたマウス座標を保持
	static Vector2 mousePosInViewport_;

	std::unique_ptr<PostProcess> postProcess_;
	std::unique_ptr<SkinningDebugWindow> skinningDebugWindow_;
};
