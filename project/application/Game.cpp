#include "Game.h"
#include "scene/SceneManager.h"
#include "scene/SceneFactory.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "base/ImGuiManager.h"
#include "scene/GamePlayScene.h" 
#include "audio/Audio.h"
#include "3d/Object3d.h"
#include "2d/SpriteCommon.h"
#include "debug/EngineDebugWindowManager.h"
#include "debug/PostEffectDebugWindow.h"
#include "debug/SkinningDebugWindow.h"
#include "debug/CG4EvaluationWindow.h"
#include "effect/ParticleManager.h"
#include "effect/PostEffectSystem.h"
#include "base/Logger.h"
#include "base/FrameClock.h"
#include "io/Input.h"
#include <algorithm>
#include <cassert>
#include <cmath>

Vector2 Game::mousePosInViewport_ = { 0, 0 };

Game::Game() = default;
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();
	postEffectSystem_ = std::make_unique<PostEffectSystem>();
	if (!postEffectSystem_->Initialize(dxCommon_.get(), srvManager_.get())) {
		Logger::Log("Game::Initialize failed to initialize PostEffectSystem.");
		assert(false && "PostEffectSystem initialization failed");
	}
#ifdef USE_IMGUI
	skinningDebugWindow_ = std::make_unique<SkinningDebugWindow>();
	engineDebugWindowManager_ = std::make_unique<EngineDebugWindowManager>();
	postEffectDebugWindow_ = std::make_unique<PostEffectDebugWindow>();
	cg4EvaluationWindow_ = std::make_unique<CG4EvaluationWindow>();
#endif

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Finalize() {
#ifdef USE_IMGUI
	cg4EvaluationWindow_.reset();
	postEffectDebugWindow_.reset();
	skinningDebugWindow_.reset();
	engineDebugWindowManager_.reset();
#endif
	SceneManager::DeleteInstance();
	if (postEffectSystem_) {
		postEffectSystem_->Finalize();
		postEffectSystem_.reset();
	}
	Framework::Finalize();
}

void Game::Update() {
	Framework::Update();
	SceneManager::GetInstance()->BeginFrame();
	postEffectSystem_->Update(frameClock_->GetFrameDeltaSeconds());

#ifdef USE_IMGUI
	if (input_ && input_->TriggerKey(DIK_F1)) {
		runtimeViewMode_ = runtimeViewMode_ == RuntimeViewMode::Debug
			? RuntimeViewMode::Play
			: RuntimeViewMode::Debug;
		if (runtimeViewMode_ == RuntimeViewMode::Play) {
			playModeInitializationPending_ = true;
		}
	}
#endif

	ImGuiManager::GetInstance()->Begin();
	SceneManager::GetInstance()->PrepareFixedUpdate();
	while (frameClock_->ConsumeFixedStep()) {
		SceneManager::GetInstance()->FixedUpdate(frameClock_->GetFixedDeltaSeconds());
	}
#ifdef USE_IMGUI
	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);
	if (runtimeViewMode_ == RuntimeViewMode::Debug) {
	if (cg4EvaluationWindow_) {
		const bool evaluationMode = !showLegacyDebugWindows_;
		if (const std::optional<bool> requestedMode = cg4EvaluationWindow_->DrawModeBar(evaluationMode)) {
			showLegacyDebugWindows_ = !*requestedMode;
		}
	}
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	if (showLegacyDebugWindows_ && engineDebugWindowManager_ && input_) {
		engineDebugWindowManager_->Draw(*input_);
	}

	if (playScene) {
		playScene->viewportHovered_ = false;
		playScene->viewportImageSize_ = { 0.0f, 0.0f };
	}

	// Game Viewport displays the gamma-corrected final display texture.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Game Viewport")) {
		ImVec2 contentSize = ImGui::GetContentRegionAvail();
		if (contentSize.x > 1.0f && contentSize.y > 1.0f) {
			float targetAspect = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
			ImVec2 displaySize = contentSize;
			if (displaySize.x / displaySize.y > targetAspect) displaySize.x = displaySize.y * targetAspect;
			else displaySize.y = displaySize.x / targetAspect;

			if (displaySize.x > 1.0f && displaySize.y > 1.0f) {
				ImVec2 offset = { (contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f };
				ImGui::SetCursorPos({ ImGui::GetCursorPos().x + offset.x, ImGui::GetCursorPos().y + offset.y });

				ImGui::Image(
					(ImTextureID)srvManager_->GetGPUDescriptorHandle(postEffectSystem_->GetFinalDisplaySrvIndex()).ptr,
					displaySize);
				ImVec2 mousePos = ImGui::GetIO().MousePos;
				ImVec2 imageTopLeft = ImGui::GetItemRectMin();
				ImVec2 imageBottomRight = ImGui::GetItemRectMax();
				ImVec2 actualDisplaySize = {
					imageBottomRight.x - imageTopLeft.x,
					imageBottomRight.y - imageTopLeft.y
				};
				mousePosInViewport_.x =
					(mousePos.x - imageTopLeft.x) / actualDisplaySize.x * static_cast<float>(WinApp::kClientWidth);
				mousePosInViewport_.y =
					(mousePos.y - imageTopLeft.y) / actualDisplaySize.y * static_cast<float>(WinApp::kClientHeight);
				if (playScene) {
					playScene->viewportImageTopLeft_ = { imageTopLeft.x, imageTopLeft.y };
					playScene->viewportImageSize_ = { actualDisplaySize.x, actualDisplaySize.y };
					playScene->viewportMousePosition_ = { mousePos.x, mousePos.y };
					playScene->viewportHovered_ = ImGui::IsItemHovered();
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	if (playScene && cg4EvaluationWindow_) {
		CG4EvaluationViewData viewData = playScene->BuildCG4EvaluationViewData();
		CG4EvaluationActions actions;
		if (!showLegacyDebugWindows_) {
			actions = cg4EvaluationWindow_->Draw(viewData);
		}
		actions.showLegacyTools = showLegacyDebugWindows_;
		playScene->ApplyCG4EvaluationActions(actions);
	}

	if (playScene && showLegacyDebugWindows_) {
		ImGui::Begin("Global Settings");
		static const char* targets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
		ImGui::Combo("Edit Focus", &playScene->selectedTarget_, targets, 5);
		ImGui::End();

		ImGui::Begin("Visibility & Cull");
		ImGui::Checkbox("Terrain", &playScene->showTerrain_);
		ImGui::Checkbox("Sphere", &playScene->showSphere_);
		ImGui::Checkbox("Plane", &playScene->showPlane_);
		ImGui::Checkbox("Sprite (2D)", &playScene->showSprite_);
		ImGui::Checkbox("Particles", &playScene->showParticles_);
		ImGui::Checkbox("Animated Model", &playScene->showAnimModel_);
		ImGui::Checkbox("Skeleton Debug", &playScene->showSkeleton_);
		ImGui::Separator();
		static const char* cullItems[] = { "None (両面)", "Front (前面削除)", "Back (背面削除)" };
		ImGui::Combo("Cull Mode", &playScene->cullMode_, cullItems, 3);
		ImGui::End();

		ImGui::Begin("Camera Control");
		ImGui::DragFloat3("Camera Position", &playScene->cameraPos_.x, 0.1f);
		ImGui::DragFloat3("Camera Rotation", &playScene->cameraRot_.x, 0.01f);
		ImGui::DragFloat("Move Speed", &playScene->cameraMoveSpeed_, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Rotate Speed", &playScene->cameraRotateSpeed_, 0.01f, 0.0f, 10.0f);
		playScene->cameraMoveSpeed_ = (std::max)(playScene->cameraMoveSpeed_, 0.0f);
		playScene->cameraRotateSpeed_ = (std::max)(playScene->cameraRotateSpeed_, 0.0f);
		if (ImGui::Button("Reset Camera")) {
			playScene->ResetCamera();
		}
		ImGui::End();

		ImGui::Begin("Object Editor");
		// 既存の平行光源
		if (ImGui::CollapsingHeader("Directional Light")) {
			ImGui::DragFloat3("Direction", &playScene->lightDirection_.x, 0.01f, -1.0f, 1.0f);
			// 正規化処理を追加
			if (ImGui::DragFloat3("D-Light Direction", &playScene->lightDirection_.x, 0.01f, -1.0f, 1.0f)) {
				if (MatrixMath::Length(playScene->lightDirection_) > 0.0f) {
					playScene->lightDirection_ = MatrixMath::Normalize(playScene->lightDirection_);
				}
			}
			ImGui::ColorEdit3("Color", &playScene->lightColor_.x);
			ImGui::SliderFloat("Intensity", &playScene->lightIntensity_, 0.0f, 10.0f);
		}

		// ★スポットライト
		if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::ColorEdit3("S-Light Color", &playScene->spotLightColor_.x);
			ImGui::DragFloat3("S-Light Position", &playScene->spotLightPos_.x, 0.1f);
			ImGui::SliderFloat("S-Light Intensity", &playScene->spotLightIntensity_, 0.0f, 20.0f);
			// 正規化処理を追加
			if (ImGui::DragFloat3("S-Light Direction", &playScene->spotLightDir_.x, 0.01f, -1.0f, 1.0f)) {
				if (MatrixMath::Length(playScene->spotLightDir_) > 0.0f) {
					playScene->spotLightDir_ = MatrixMath::Normalize(playScene->spotLightDir_);
				}
			}
			ImGui::SliderFloat("S-Light Distance", &playScene->spotLightDistance_, 1.0f, 100.0f);
			ImGui::SliderFloat("S-Light Decay", &playScene->spotLightDecay_, 0.1f, 10.0f);

			// 角度は度数法(Degree)で調整し、内部でラジアンに変換する
			static float angleDeg = 30.0f;
			if (ImGui::SliderFloat("Beam Angle", &angleDeg, 0.0f, 90.0f)) {
				playScene->spotLightAngle_ = angleDeg * (3.141592f / 180.0f);
			}
			static float falloffDeg = 10.0f;
			if (ImGui::SliderFloat("Falloff Start", &falloffDeg, 0.0f, 90.0f)) {
				playScene->spotLightFalloff_ = falloffDeg * (3.141592f / 180.0f);
			}
		}

		if (ImGui::CollapsingHeader("Sphere (3D)")) {
			if (ImGui::SliderFloat("Radius", &playScene->sphereRadius_, 0.1f, 10.0f)) playScene->CreateSphere(playScene->sphereRadius_);
			ImGui::DragFloat3("Position", &playScene->spherePos_.x, 0.1f);
			ImGui::DragFloat3("Rotation", &playScene->objectRot_.x, 0.01f);
		}

		if (ImGui::CollapsingHeader("Animation Control", ImGuiTreeNodeFlags_DefaultOpen)) {
			// 配布データを含むアニメーションモデルの動的切り替えUI
			static const char* animModelNames[] = { "AnimatedCube (仮)", "simpleSkin (配布データ)", "human/walk (配布データ)", "human/sneakWalk (配布データ)" };
			if (ImGui::Combo("Model Select", &playScene->currentAnimModelIdx_, animModelNames, 4)) {
				playScene->ChangeAnimationModel(playScene->currentAnimModelIdx_);
			}
			ImGui::Checkbox("Show Model", &playScene->showAnimModel_);
			if (playScene->animObj_) {
				ImGui::Checkbox("Play Animation", &playScene->animObj_->GetIsAnimationPlaying());
				ImGui::SliderFloat("Speed", &playScene->animObj_->GetAnimationSpeed(), -5.0f, 5.0f);
				float duration = playScene->animObj_->GetAnimation().duration;
				if (duration > 0.0f) {
					ImGui::SliderFloat("Time", &playScene->animObj_->GetAnimationTime(), 0.0f, duration);
				}
			}
		}

		// === Cylinderパラメータ（資料「Cylinderの拡張」対応） ===
		if (ImGui::CollapsingHeader("Cylinder (Effect)", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool changed = false;
			// 上面の半径（0にすると円錐になる）
			changed |= ImGui::SliderFloat("Top Radius", &playScene->cylTopRadius_, 0.0f, 5.0f);
			// 下面の半径
			changed |= ImGui::SliderFloat("Bottom Radius", &playScene->cylBottomRadius_, 0.0f, 5.0f);
			// 高さ
			changed |= ImGui::SliderFloat("Cyl Height", &playScene->cylHeight_, 0.1f, 10.0f);
			// 円周方向の分割数
			changed |= ImGui::SliderInt("Segments", &playScene->cylSegments_, 3, 64);
			// 高さ方向の分割数（頂点カラーグラデーション用）
			changed |= ImGui::SliderInt("Vert Divisions", &playScene->cylVertDivisions_, 1, 16);
			// パラメータが変わったらメッシュを再生成
			if (changed) {
				playScene->RebuildCylinder();
			}
		}

		if (playScene->animObj_ && playScene->animObj_->GetSkeleton()) {
			if (ImGui::CollapsingHeader("Skeleton Bones Control", ImGuiTreeNodeFlags_DefaultOpen)) {
				// 参照を変数として取得
				auto& skeletonOpt = playScene->animObj_->GetSkeleton();
				if (skeletonOpt.has_value()) {
					Skeleton& skeleton = skeletonOpt.value();

					// 1. ジョイント名の一覧を作ってコンボボックスにする
					static int selectedJointIdx = 0;
					std::vector<const char*> jointNames;
					for (const auto& joint : skeleton.joints) {
						jointNames.push_back(joint.name.c_str());
					}

					if (selectedJointIdx >= static_cast<int>(jointNames.size())) {
						selectedJointIdx = 0;
					}

					ImGui::Combo("Target Joint", &selectedJointIdx, jointNames.data(), static_cast<int>(jointNames.size()));

					ImGui::Separator();

					// 2. 選択されたジョイントのトランスフォームを直接いじる
					if (!skeleton.joints.empty()) {
						Joint& activeJoint = skeleton.joints[selectedJointIdx];

						ImGui::Text("Active: %s (Index: %d)", activeJoint.name.c_str(), activeJoint.index);
						ImGui::DragFloat3("Bone Translate", &activeJoint.transform.translate.x, 0.05f);
						ImGui::DragFloat3("Bone Scale", &activeJoint.transform.scale.x, 0.05f, 0.01f, 10.0f);

						// 回転をクォータニオンへ安全に反映させるための補助UI
						static Vector3 boneEulerDeg = { 0.0f, 0.0f, 0.0f };
						if (ImGui::DragFloat3("Bone Rotate (Euler)", &boneEulerDeg.x, 0.5f)) {
							// ここで必要に応じてオイラー角からクォータニオンを再構築して activeJoint.transform.rotate に代入します
						}
					}
				}
			}
		}
		ImGui::End();

		if (skinningDebugWindow_) {
			skinningDebugWindow_->Draw(playScene->animObj_.get());
		}

		ImGui::Begin("Effect Control");

		ImGui::Text("P Key: Emit");
		const char* pTypes[] = { "Spark (Manual)", "Ring (Model)", "Cylinder (Primitive)", "Combined", "Explosion (Emit)" };
		ImGui::Combo("Particle Mode", &playScene->activeParticleType_, pTypes, 5);

		ImGui::Separator();
		const char* gpuParticleModeItems[] = { "Off", "Agriculture Mode", "Particle Interaction Mode" };
		int gpuParticleModeIndex = static_cast<int>(playScene->gpuParticleDebugMode_);
		if (ImGui::Combo("GPUParticle Mode", &gpuParticleModeIndex, gpuParticleModeItems, _countof(gpuParticleModeItems))) {
			playScene->SetGPUParticleDebugMode(static_cast<GamePlayScene::GPUParticleDebugMode>(gpuParticleModeIndex));
		}

		if (playScene->gpuParticleDebugMode_ == GamePlayScene::GPUParticleDebugMode::Agriculture) {
			if (ImGui::CollapsingHeader("Agriculture Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat3("Emit Position", &playScene->agricultureEmitPosition_.x, 0.1f);
				ImGui::SliderFloat("Particle Size", &playScene->agricultureParticleSize_, 0.02f, 1.0f);
				ImGui::SliderInt("Count", &playScene->agricultureParticleCount_, 1, 1024);

				if (ImGui::Button("Dirt Dust")) {
					playScene->EmitAgricultureParticle(GamePlayScene::AgricultureParticleType::DirtDust);
				}
				ImGui::SameLine();
				if (ImGui::Button("Water Splash")) {
					playScene->EmitAgricultureParticle(GamePlayScene::AgricultureParticleType::WaterSplash);
				}
				ImGui::SameLine();
				if (ImGui::Button("Harvest Sparkle")) {
					playScene->EmitAgricultureParticle(GamePlayScene::AgricultureParticleType::HarvestSparkle);
				}

				ImGui::Checkbox("Show Key Guide", &playScene->agricultureShowKeyGuide_);
				if (playScene->agricultureShowKeyGuide_) {
					ImGui::TextUnformatted("1: Dirt Dust  2: Water Splash  3: Harvest Sparkle");
					ImGui::TextUnformatted("4: Pollen / Spore  5: Bug Swarm");
				}
			}
		} else if (playScene->gpuParticleDebugMode_ == GamePlayScene::GPUParticleDebugMode::Interaction) {
			if (ImGui::CollapsingHeader("Interaction Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool changed = false;
				changed |= ImGui::SliderInt("Particle Grid Count", &playScene->interactionGridCount_, 2, 10);
				changed |= ImGui::SliderFloat("Particle Size", &playScene->interactionParticleSize_, 0.02f, 0.05f);
				ImGui::SliderFloat("Brush Radius", &playScene->interactionBrushRadius_, 0.1f, 10.0f);
				ImGui::SliderFloat("Brush Strength", &playScene->interactionBrushStrength_, 0.0f, 10.0f);
				ImGui::SliderFloat("Interaction Damping", &playScene->interactionDamping_, 0.80f, 0.995f);
				playScene->interactionGridCount_ = std::clamp(playScene->interactionGridCount_, 2, 10);
				playScene->interactionDamping_ = std::clamp(playScene->interactionDamping_, 0.80f, 0.995f);
				playScene->interactionParticleCount_ = playScene->CalculateInteractionParticleCount();
				ImGui::Text("Current Particle Count: %u", playScene->interactionParticleCount_);
				ImGui::Text("Brush Position: %.2f, %.2f, %.2f",
					playScene->interactionBrushPosition_.x,
					playScene->interactionBrushPosition_.y,
					playScene->interactionBrushPosition_.z);
				if (changed) {
					playScene->interactionResetRequested_ = true;
				}
				if (ImGui::Button("Reset Grid")) {
					Framework::GetInstance()->GetParticleManager()->ResetGPUParticles();
					playScene->interactionResetRequested_ = true;
				}
				ImGui::TextUnformatted("Left Click on Game Viewport: Push");
				ImGui::TextUnformatted("Shift + Left Click: Pull");
			}
		}

		// === discard しきい値（資料スライド10対応） ===
		// この値以下のアルファを持つピクセルが棄却される
		// 0.0 = 完全透明のみ棄却（従来動作）
		// 0.5 = 半透明以下を棄却（くっきりした輪郭になる）
		static float alphaRef = 0.0f;
		if (ImGui::SliderFloat("Alpha Discard", &alphaRef, 0.0f, 1.0f)) {
			Framework::GetInstance()->GetParticleManager()->SetAlphaReference(alphaRef);
		}

		ImGui::Separator();
		if (ImGui::Button("Clear All Particles")) {
			Framework::GetInstance()->GetParticleManager()->ClearAll();
		}

		ImGui::End();

		if (postEffectDebugWindow_ && postEffectSystem_) {
			const PostEffectDebugActions actions = postEffectDebugWindow_->Draw(*postEffectSystem_);
			if (actions.resetSettings) {
				postEffectSystem_->ResetSettings();
			} else if (actions.settings.has_value()) {
				postEffectSystem_->GetSettings() = *actions.settings;
			}
			if (actions.chainModeEnabled.has_value()) {
				postEffectSystem_->SetChainModeEnabled(*actions.chainModeEnabled);
			}
			if (actions.chainPassChange.has_value()) {
				postEffectSystem_->SetChainPassEnabled(
					actions.chainPassChange->index,
					actions.chainPassChange->enabled);
			}
		}
	}
	} else if (playScene) {
		// Play表示ではImGuiを提出せず、入力座標をクライアント全体へ対応させる。
		playScene->viewportHovered_ = true;
		playScene->viewportImageTopLeft_ = { 0.0f, 0.0f };
		playScene->viewportImageSize_ = {
			static_cast<float>(WinApp::kClientWidth),
			static_cast<float>(WinApp::kClientHeight)
		};
		if (input_) {
			const Vector2 mousePosition = input_->GetMousePosition();
			playScene->viewportMousePosition_ = mousePosition;
			mousePosInViewport_ = mousePosition;
		}

		CG4EvaluationActions actions;
		actions.showLegacyTools = false;
		if (playModeInitializationPending_) {
			actions.preset = CG4EvaluationPreset::Gameplay;
			playModeInitializationPending_ = false;
		}
		playScene->ApplyCG4EvaluationActions(actions);
	}
#endif

	SceneManager::GetInstance()->Update();
	ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	// Draw the scene into the offscreen RenderTexture.
	SceneManager::GetInstance()->Draw();

	float nearClip = 0.1f;
	float farClip = 1000.0f;
	if (BaseScene* currentScene = SceneManager::GetInstance()->GetCurrentScene()) {
		if (GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(currentScene)) {
			if (playScene->camera_) {
				nearClip = playScene->camera_->GetNearClip();
				farClip = playScene->camera_->GetFarClip();
			}
		}
	}
	postEffectSystem_->Execute(nearClip, farClip);

	// Render ImGui last on the swapchain, then present.
	ImGuiManager::GetInstance()->Draw();
	dxCommon_->RestoreRenderTextureToRenderTarget();
	dxCommon_->PostDraw();
}
