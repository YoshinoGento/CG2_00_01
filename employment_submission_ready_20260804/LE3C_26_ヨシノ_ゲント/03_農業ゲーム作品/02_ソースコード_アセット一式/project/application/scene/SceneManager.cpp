#include "SceneManager.h"
#include "AbstractSceneFactory.h"
#include "2d/TextureManager.h"
#include <cassert>

SceneManager* SceneManager::instance = nullptr;

SceneManager* SceneManager::GetInstance() {
	if (instance == nullptr) instance = new SceneManager();
	return instance;
}

void SceneManager::DeleteInstance() {
	if (instance) { delete instance; instance = nullptr; }
}

SceneManager::~SceneManager() {
	if (scene_) scene_->Finalize();
	// unique_ptr なので手動の delete は不要です
}

/**
 * 更新処理
 * 予約された次のシーン（nextScene_）がある場合、入れ替えを行います。
 */
void SceneManager::BeginFrame() {
	if (nextScene_) {
		if (scene_) {
			scene_->Finalize();
			// PostDraw waits for the previous frame fence, so Update is the safe descriptor/resource release boundary.
			TextureManager::GetInstance()->ReleaseSceneTextures();
		}

		// ★所有権を移動。この代入の瞬間に古い scene_ は自動で破棄されます。
		scene_ = std::move(nextScene_);
		nextScene_ = nullptr;

		scene_->SetSceneManager(this);
		scene_->Initialize();
	}
}

void SceneManager::FixedUpdate(float fixedDeltaTime) {
	if (scene_) {
		scene_->FixedUpdate(fixedDeltaTime);
	}
}

void SceneManager::PrepareFixedUpdate() {
	if (scene_) {
		scene_->PrepareFixedUpdate();
	}
}

void SceneManager::Update() {
	if (scene_) {
		scene_->Update();
	}
}

void SceneManager::Draw() {
	if (scene_) {
		scene_->Draw();
	}
}

/**
 * 名前指定によるシーン切り替え
 */
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_ && "SceneFactory is not set.");

	// ★工場(CreateScene)が返す unique_ptr を、そのまま予約用関数の引数へ渡します
	SetNextScene(sceneFactory_->CreateScene(sceneName));
}
