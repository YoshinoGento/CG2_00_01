#include "SceneManager.h"
#include "AbstractSceneFactory.h"
#include <cassert>

// シングルトン実体の初期化
SceneManager* SceneManager::instance = nullptr;

SceneManager* SceneManager::GetInstance() {
	if (instance == nullptr) {
		instance = new SceneManager();
	}
	return instance;
}

void SceneManager::DeleteInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

/**
 * デストラクタ：終了時に実行中のシーンを確実に破棄する
 */
SceneManager::~SceneManager() {
	if (scene_) {
		scene_->Finalize();
		delete scene_;
	}
}

/**
 * 更新処理：シーンの切り替え依頼（予約）があれば実行する
 */
void SceneManager::Update() {
	// 次のシーンが予約されている場合
	if (nextScene_) {
		// 1. 古いシーンがあれば後片付けして削除
		if (scene_) {
			scene_->Finalize();
			delete scene_;
		}

		// 2. 新しいシーンに入れ替え
		scene_ = nextScene_;
		nextScene_ = nullptr;

		// 3. 新しいシーンに「自分（マネージャ）」を教える（スライド27準拠）
		scene_->SetSceneManager(this);

		// 4. 新しいシーンの初期化
		scene_->Initialize();
	}

	// ★重要：実行中のシーンが存在する場合のみUpdateを呼ぶ
	// これがないと scene_ が nullptr の時に 0x28 エラーで落ちます
	if (scene_ != nullptr) {
		scene_->Update();
	}
}

/**
 * 描画処理
 */
void SceneManager::Draw() {
	// ★重要：実行中のシーンが存在する場合のみDrawを呼ぶ
	// 切り替えの瞬間に一瞬だけ空になる可能性があるため、このチェックは必須です
	if (scene_ != nullptr) {
		scene_->Draw();
	}
}

/**
 * 名前指定によるシーン切り替え
 */
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_ && "SceneFactoryが登録されていません。Game::Initializeを確認してください。");

	// 工場を使って新しいシーンを作り、予約する
	SetNextScene(sceneFactory_->CreateScene(sceneName));
}