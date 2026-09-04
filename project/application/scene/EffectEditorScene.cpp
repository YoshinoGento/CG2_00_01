#include "application/scene/EffectEditorScene.h"

#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "base/FrameClock.h"
#include "base/Framework.h"
#include "base/SrvManager.h"
#include "effect/ParticleManager.h"
#include "effect/ComicTextEffect.h"
#ifdef USE_IMGUI
#include "debug/ComicTextEffectEditor.h"
#include "debug/ParticleEffectEditor.h"
#include "externals/imgui/imgui.h"
#endif

#include <cassert>

void EffectEditorScene::Initialize() {
	framework_ = Framework::GetInstance();
	assert(framework_ && "Framework must exist before EffectEditorScene initialization.");

	camera_ = std::make_unique<Camera>();
	camera_->SetTranslate({ 0.0f, 4.5f, -9.0f });
	camera_->SetRotate({ 0.28f, 0.0f, 0.0f });
	camera_->Update();
	LineDrawer::GetInstance()->Initialize(framework_->GetDxCommon());
	comicTextEffects_ = std::make_unique<ComicTextEffectSystem>();
	comicTextEffects_->Initialize(framework_->GetSpriteCommon());
#ifdef USE_IMGUI
	particleEffectEditor_ = std::make_unique<ParticleEffectEditor>();
	comicTextEffectEditor_ = std::make_unique<ComicTextEffectEditor>();
#endif
}

void EffectEditorScene::Finalize() {
#ifdef USE_IMGUI
	particleEffectEditor_.reset();
	comicTextEffectEditor_.reset();
#endif
	comicTextEffects_.reset();
	if (framework_ && framework_->GetParticleManager()) {
		framework_->GetParticleManager()->ClearAll();
		framework_->GetParticleManager()->ResetGPUParticles();
	}
	camera_.reset();
	framework_ = nullptr;
}

void EffectEditorScene::Update() {
	if (!framework_ || !camera_) {
		return;
	}
	camera_->Update();
	if (ParticleManager* particles = framework_->GetParticleManager()) {
		const FrameClock* clock = framework_->GetFrameClock();
		particles->Update(
			camera_.get(),
			clock ? clock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds);
	}
	if (comicTextEffects_) {
		const FrameClock* clock = framework_->GetFrameClock();
		comicTextEffects_->Update(
			clock ? clock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds,
			camera_->GetViewProjectionMatrix());
	}
}

void EffectEditorScene::DrawPreviewGrid() {
	LineDrawer* lines = LineDrawer::GetInstance();
	constexpr int kGridHalfCount = 10;
	constexpr float kSpacing = 1.0f;
	const Vector4 major = { 0.22f, 0.34f, 0.48f, 1.0f };
	const Vector4 minor = { 0.12f, 0.16f, 0.22f, 1.0f };
	for (int index = -kGridHalfCount; index <= kGridHalfCount; ++index) {
		const float offset = static_cast<float>(index) * kSpacing;
		const Vector4 color = index == 0 ? major : minor;
		lines->DrawLine({ -10.0f, 0.0f, offset + previewPosition_.z },
			{ 10.0f, 0.0f, offset + previewPosition_.z }, color);
		lines->DrawLine({ offset, 0.0f, previewPosition_.z - 10.0f },
			{ offset, 0.0f, previewPosition_.z + 10.0f }, color);
	}
	lines->DrawWireSphere(previewPosition_, 0.15f, { 1.0f, 0.35f, 0.85f, 1.0f }, 16);
}

void EffectEditorScene::Draw() {
	if (!framework_ || !camera_) {
		return;
	}
	DrawPreviewGrid();
	if (ParticleManager* particles = framework_->GetParticleManager()) {
		particles->Draw();
	}
	LineDrawer::GetInstance()->Draw(camera_->GetViewProjectionMatrix());
	if (comicTextEffects_) {
		comicTextEffects_->Draw();
	}
}

void EffectEditorScene::DrawEditorUi(const SceneEditorContext& context) {
#ifdef USE_IMGUI
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float controlsWidth = viewport->WorkSize.x * 0.32f;

	ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(
		{ viewport->WorkSize.x - controlsWidth, viewport->WorkSize.y },
		ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("エフェクトプレビュー###EffectEditorViewport")) {
		const ImVec2 available = ImGui::GetContentRegionAvail();
		if (context.srvManager &&
			context.srvManager->IsAllocated(context.finalDisplaySrvIndex) &&
			available.x > 1.0f && available.y > 1.0f) {
			const float targetAspect = context.virtualWidth / context.virtualHeight;
			ImVec2 imageSize = available;
			if (imageSize.x / imageSize.y > targetAspect) {
				imageSize.x = imageSize.y * targetAspect;
			} else {
				imageSize.y = imageSize.x / targetAspect;
			}
			ImGui::Image(
				static_cast<ImTextureID>(context.srvManager->GetGPUDescriptorHandle(
					context.finalDisplaySrvIndex).ptr),
				imageSize);
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::SetNextWindowPos(
		{ viewport->WorkPos.x + viewport->WorkSize.x - controlsWidth, viewport->WorkPos.y },
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(
		{ controlsWidth, viewport->WorkSize.y },
		ImGuiCond_FirstUseEver);
	if (ImGui::Begin("エフェクト設定###EffectEditorControls")) {
		ImGui::TextUnformatted("JSONプリセットを作成してゲーム中から名前で再生できます。");
		ImGui::DragFloat3("Preview Origin", &previewPosition_.x, 0.05f, -50.0f, 50.0f);
		if (ImGui::BeginTabBar("EffectEditorTabs")) {
			if (ImGui::BeginTabItem("Particles")) {
				if (particleEffectEditor_ && framework_ && framework_->GetParticleManager()) {
					const FrameClock* clock = framework_->GetFrameClock();
					particleEffectEditor_->Draw(*framework_->GetParticleManager(), previewPosition_,
						clock ? clock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds);
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Comic Text")) {
				if (comicTextEffectEditor_ && comicTextEffects_) {
					comicTextEffectEditor_->Draw(*comicTextEffects_, previewPosition_);
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
#else
	(void)context;
#endif
}
