#pragma once

#include "application/scene/BaseScene.h"
#include "math/Struct.h"
#include "effect/ComicTextEffect.h"
#include "effect/ProceduralCombatEffect.h"
#ifdef USE_IMGUI
#include "debug/ComicTextEffectEditor.h"
#include "debug/CombatEffectEditor.h"
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
	std::unique_ptr<LightningEffect> lightningEffect_;
	std::unique_ptr<SlashEffect> slashEffect_;
	Vector3 previewPosition_ = { 0.0f, 1.0f, 6.0f };
#ifdef USE_IMGUI
	bool hitParticleEnabled_ = true;
	bool hitComicTextEnabled_ = true;
	bool hitLightningEnabled_ = true;
	bool hitSlashEnabled_ = true;
	std::unique_ptr<ParticleEffectEditor> particleEffectEditor_;
	std::unique_ptr<ComicTextEffectEditor> comicTextEffectEditor_;
	std::unique_ptr<CombatEffectEditor> combatEffectEditor_;
#endif
};
