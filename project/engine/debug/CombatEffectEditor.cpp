#include "debug/CombatEffectEditor.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"

void CombatEffectEditor::DrawLightning(LightningEffect& effect, const Vector3& origin,
	bool previewRequested) {
	ImGui::TextUnformatted("Procedural lightning / dragon-energy editor");
	ImGui::DragFloat3("Direction##Lightning", &lightning_.direction.x, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("Length##Lightning", &lightning_.length, 0.05f, 0.1f, 100.0f);
	ImGui::DragFloat("Overall Size##Lightning", &lightning_.size, 0.02f, 0.05f, 20.0f);
	ImGui::DragFloat("Bolt Width##Lightning", &lightning_.width, 0.005f, 0.001f, 3.0f);
	ImGui::DragFloat("Jaggedness##Lightning", &lightning_.jaggedness, 0.01f, 0.0f, 10.0f);
	ImGui::SliderInt("Segments##Lightning", &lightning_.segments, 2, 128);
	ImGui::SliderInt("Branches##Lightning", &lightning_.branches, 0, 32);
	ImGui::SliderFloat("Branch Length##Lightning", &lightning_.branchLength, 0.05f, 1.0f);
	ImGui::DragFloat("Flicker Speed##Lightning", &lightning_.flickerSpeed, 0.5f, 0.0f, 120.0f);
	ImGui::SliderFloat("Duration##Lightning", &lightning_.duration, 0.03f, 5.0f, "%.2f sec");
	ImGui::ColorEdit4("Glow Color##Lightning", &lightning_.glowColor.x);
	ImGui::ColorEdit4("Core Color##Lightning", &lightning_.coreColor.x);
	int seed = static_cast<int>(lightning_.randomSeed);
	if (ImGui::InputInt("Random Seed##Lightning", &seed)) lightning_.randomSeed = static_cast<uint32_t>(seed < 0 ? 0 : seed);
	if (ImGui::Button("Thunder preset")) {
		lightning_.glowColor = { 0.18f, 0.45f, 1.0f, 0.65f };
		lightning_.coreColor = { 0.85f, 0.95f, 1.0f, 1.0f };
		lightning_.jaggedness = 0.55f;
	}
	ImGui::SameLine();
	if (ImGui::Button("Dragon preset")) {
		lightning_.glowColor = { 0.45f, 0.01f, 0.06f, 0.8f };
		lightning_.coreColor = { 1.0f, 0.12f, 0.18f, 1.0f };
		lightning_.jaggedness = 0.85f;
		lightning_.branches = 8;
	}
	if (ImGui::Button("Preview Lightning", ImVec2(170.0f, 0.0f)) || previewRequested) effect.Play(lightning_, origin);
	ImGui::SameLine();
	if (ImGui::Button("Clear##Lightning")) effect.Clear();
	ImGui::TextDisabled("Included in the composed hit preview when enabled.");
}

void CombatEffectEditor::DrawSlash(SlashEffect& effect, const Vector3& origin,
	bool previewRequested) {
	ImGui::TextUnformatted("Procedural slash-arc editor");
	ImGui::DragFloat("Overall Size##Slash", &slash_.size, 0.02f, 0.05f, 20.0f);
	ImGui::DragFloat("Radius##Slash", &slash_.radius, 0.05f, 0.1f, 50.0f);
	ImGui::SliderFloat("Arc Angle##Slash", &slash_.arcDegrees, 5.0f, 355.0f, "%.0f deg");
	ImGui::SliderFloat("Rotation##Slash", &slash_.rotationDegrees, -180.0f, 180.0f, "%.0f deg");
	ImGui::DragFloat("Blade Thickness##Slash", &slash_.thickness, 0.005f, 0.001f, 5.0f);
	ImGui::SliderInt("Smoothness##Slash", &slash_.segments, 3, 160);
	ImGui::SliderInt("Afterimages##Slash", &slash_.afterimages, 0, 16);
	ImGui::SliderFloat("Trail Spacing##Slash", &slash_.afterimageSpacingDegrees, 0.0f, 45.0f, "%.1f deg");
	ImGui::SliderFloat("Duration##Slash", &slash_.duration, 0.03f, 5.0f, "%.2f sec");
	ImGui::ColorEdit4("Outer Color##Slash", &slash_.outerColor.x);
	ImGui::ColorEdit4("Core Color##Slash", &slash_.coreColor.x);
	if (ImGui::Button("Preview Slash", ImVec2(170.0f, 0.0f)) || previewRequested) effect.Play(slash_, origin);
	ImGui::SameLine();
	if (ImGui::Button("Clear##Slash")) effect.Clear();
	ImGui::TextDisabled("Included in the composed hit preview when enabled.");
}

#endif
