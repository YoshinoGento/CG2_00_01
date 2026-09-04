#pragma once

#include "application/scene/BaseScene.h"
#include "math/Struct.h"
#include "effect/ComicTextEffect.h"
#ifdef USE_IMGUI
#include "debug/ComicTextEffectEditor.h"
#endif

#include <memory>

class Camera;
class Framework;
#ifdef USE_IMGUI
class ParticleEffectEditor;
#endif

// パーティクルエフェクトの作成と確認だけに集中する専用シーン。
class EffectEditorScene final : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	bool UsesEditorShell() const noexcept override { return false; }
	void DrawEditorUi(const SceneEditorContext& context) override;

private:
	void DrawPreviewGrid();

	Framework* framework_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<ComicTextEffectSystem> comicTextEffects_;
	Vector3 previewPosition_ = { 0.0f, 1.0f, 6.0f };
#ifdef USE_IMGUI
	std::unique_ptr<ParticleEffectEditor> particleEffectEditor_;
	std::unique_ptr<ComicTextEffectEditor> comicTextEffectEditor_;
#endif
};
