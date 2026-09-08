#include "debug/EngineDebugWindowManager.h"

#include "externals/imgui/imgui.h"
#include "io/Input.h"

void EngineDebugWindowManager::Draw(const Input& input)
{
	if (ImGui::Begin("Engine Debug")) {
		ImGui::Checkbox("Input Debug", &showInputDebug_);
		ImGui::Checkbox("2D Feature Debug", &show2DFeatureDebug_);
	}
	ImGui::End();

	if (show2DFeatureDebug_) {
		engine2DFeatureDebugWindow_.Draw();
	}
	if (showInputDebug_) {
		inputDebugWindow_.Draw(input);
	}
}
