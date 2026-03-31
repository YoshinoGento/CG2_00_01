#include "Game.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include <cmath>

// 静的メンバの初期化
Vector2 Game::mousePosInViewport_ = { 0, 0 };

Game::Game() = default;
Game::~Game() = default;

/**
 * 初期化
 */
void Game::Initialize() {
	Framework::Initialize();

	// 1. ゲーム画面（テクスチャ）を ImGui で使えるように SRV 登録
	viewportSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		viewportSrvIndex_,
		dxCommon_->GetRenderTextureResource(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1
	);

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

/**
 * 終了処理
 */
void Game::Finalize() {
	SceneManager::DeleteInstance();
	Framework::Finalize();
}

/**
 * 更新
 */
void Game::Update() {
	Framework::Update();

	// ImGuiのフレーム開始
	ImGuiManager::GetInstance()->Begin();

	// 1. 全画面 DockSpace（エディタの土台）
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	// 2. 「Game Viewport」ウィンドウの作成
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Game Viewport")) {
		ImVec2 contentSize = ImGui::GetContentRegionAvail();

		// アスペクト比(16:9)維持の計算
		float targetAspect = 1280.0f / 720.0f;
		ImVec2 displaySize = contentSize;
		if (displaySize.x / displaySize.y > targetAspect) {
			displaySize.x = displaySize.y * targetAspect;
		} else {
			displaySize.y = displaySize.x / targetAspect;
		}

		// ウィンドウ内での中央寄せ
		ImVec2 offset = { (contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f };
		ImVec2 currentPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos({ currentPos.x + offset.x, currentPos.y + offset.y });

		// マウス座標の補正（スクリーン座標からゲームの 1280x720 解像度へ逆変換）
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();

		mousePosInViewport_.x = (mousePos.x - imageTopLeft.x) / displaySize.x * 1280.0f;
		mousePosInViewport_.y = (mousePos.y - imageTopLeft.y) / displaySize.y * 720.0f;

		// ゲーム画面（描画済みテクスチャ）を表示
		ImGui::Image(
			(ImTextureID)srvManager_->GetGPUDescriptorHandle(viewportSrvIndex_).ptr,
			displaySize
		);
	}
	ImGui::End();
	ImGui::PopStyleVar();

	// シーンの更新（マウス座標補正の後に呼ぶことで、最新の座標で操作できる）
	SceneManager::GetInstance()->Update();

	// ImGuiのフレーム終了
	ImGuiManager::GetInstance()->End();
}

/**
 * 描画
 */
void Game::Draw() {
	// 1. ゲーム画面（レンダーテクスチャ）への描き込み準備
	dxCommon_->PreDraw();

	// ★重要：テクスチャ描画用のヒープをセット（これがないとクラッシュします）
	srvManager_->PreDraw();

	// シーンの描画（スプライトや3Dモデルなど）
	SceneManager::GetInstance()->Draw();

	// 2. 本物のモニター（スワップチェーン）へ切り替えて ImGui を描画
	dxCommon_->PreDrawToSwapChain();

	// ImGuiの描画（この中で再度、専用のヒープセットが行われます）
	ImGuiManager::GetInstance()->Draw();

	// 3. 全ての描画コマンドを完了させて画面を表示
	dxCommon_->PostDraw();
}