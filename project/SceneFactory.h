#pragma once
#include "AbstractSceneFactory.h"

/**
 * SceneFactoryクラス
 * アプリケーション固有の具体的なシーンを作る工場です。
 */
class SceneFactory : public AbstractSceneFactory {
public:
	// 名前を元に実際のクラス（TitleSceneなど）を生成します
	BaseScene* CreateScene(const std::string& sceneName) override;
};