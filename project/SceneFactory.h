#pragma once
#include "AbstractSceneFactory.h"
#include <memory> // ★必須
#include <string>

/**
 * SceneFactoryクラス
 */
class SceneFactory : public AbstractSceneFactory {
public:
	// ★親クラスに合わせて戻り値を std::unique_ptr<BaseScene> に変更
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
};