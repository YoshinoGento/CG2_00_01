#pragma once
#include <string>
#include "BaseScene.h"

/**
 * AbstractSceneFactoryクラス（インターフェース）
 * 「シーンを作る工場」のルールを定めたクラスです。
 */
class AbstractSceneFactory {
public:
	virtual ~AbstractSceneFactory() = default;

	/**
	 * シーンを生成する
	 * @param sceneName 生成したいシーンの名前
	 * @return 作成されたシーンのインスタンス（ポインタ）
	 */
	virtual BaseScene* CreateScene(const std::string& sceneName) = 0;
};