#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"
#include "MagnetPrototypeScene.h"
#include "EffectEditorScene.h"

/**
 * シーン生成の実装
 */
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	if (sceneName == "TITLE") {
		// make_unique を使うことで、生成と同時にスマートポインタの箱に入れます
		return std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		return std::make_unique<GamePlayScene>();
	} else if (sceneName == "MAGNET_PROTOTYPE") {
		return std::make_unique<MagnetPrototypeScene>();
	} else if (sceneName == "EFFECT_EDITOR") {
		return std::make_unique<EffectEditorScene>();
	}

	return nullptr;
}
