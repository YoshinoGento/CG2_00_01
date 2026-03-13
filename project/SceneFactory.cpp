#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"

/**
 * CreateScene: 名前からシーンの実体を作成する「工場」
 */
BaseScene* SceneFactory::CreateScene(const std::string& sceneName) {
	// 作成したシーンを一時的に入れる変数
	BaseScene* newScene = nullptr;

	// 名前（文字列）をチェックして、作るクラスを決めます
	if (sceneName == "TITLE") {
		// TitleScene は BaseScene を継承しているので代入可能です
		newScene = new TitleScene();
	} else if (sceneName == "GAMEPLAY") {
		// ★GamePlayScene が BaseScene を継承したので、これでエラーが消えます！
		newScene = new GamePlayScene();
	}

	return newScene;
}