#pragma once
#include <string>
#include <memory> // ★必須：unique_ptr を使うために必要
#include "BaseScene.h"

/**
 * AbstractSceneFactory
 * シーン生成工場の抽象インターフェース
 */
class AbstractSceneFactory {
public:
	virtual ~AbstractSceneFactory() = default;

	// ★戻り値を unique_ptr に統一
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
};