#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"

/**
 * シーン生成の実装
 */
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	if (sceneName == "TITLE") {
		// make_unique を使うことで、生成と同時にスマートポインタの箱に入れます
		return std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		return std::make_unique<GamePlayScene>();
	}

	return nullptr;
}