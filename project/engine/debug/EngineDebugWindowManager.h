#pragma once

#include "debug/Engine2DFeatureDebugWindow.h"
#include "debug/InputDebugWindow.h"

class Input;

class EngineDebugWindowManager {
public:
	void Draw(const Input& input);

private:
	bool show2DFeatureDebug_ = false;
	bool showInputDebug_ = false;
	Engine2DFeatureDebugWindow engine2DFeatureDebugWindow_;
	InputDebugWindow inputDebugWindow_;
};
