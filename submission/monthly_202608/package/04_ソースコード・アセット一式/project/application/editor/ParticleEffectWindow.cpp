#include "editor/ParticleEffectWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#include <algorithm>
#include <iterator>
#endif

editor::ParticleEditorCommand ParticleEffectWindow::Draw(
	const editor::ParticleEditorViewData& viewData) {
	editor::ParticleEditorCommand command;
	command.activeParticleType = viewData.activeParticleType;
	command.gpuMode = viewData.gpuMode;
	command.agriculture = viewData.agriculture;
	command.interaction = viewData.interaction;
	command.alphaReference = viewData.alphaReference;
#ifdef USE_IMGUI
	if (!open_) {
		return command;
	}
	if (!ImGui::Begin("Particle Effect", &open_)) {
		ImGui::End();
		return command;
	}

	ImGui::TextUnformatted("Space Key: Emit");
	constexpr const char* kParticleTypes[] = {
		"Spark (Manual)",
		"Ring (Model)",
		"Cylinder (Primitive)",
		"Combined",
		"Explosion (Emit)",
	};
	command.activeParticleTypeChanged |= ImGui::Combo(
		"Particle Mode", &command.activeParticleType, kParticleTypes, 5);
	ImGui::Separator();

	constexpr const char* kGpuParticleModes[] = {
		"Off",
		"Agriculture Mode",
		"Particle Interaction Mode",
	};
	int gpuParticleModeIndex = static_cast<int>(command.gpuMode);
	if (ImGui::Combo(
		"GPU Particle Mode",
		&gpuParticleModeIndex,
		kGpuParticleModes,
		static_cast<int>(std::size(kGpuParticleModes)))) {
		command.gpuMode = static_cast<editor::EditorGpuParticleMode>(gpuParticleModeIndex);
		command.gpuModeChanged = true;
	}

	if (command.gpuMode == editor::EditorGpuParticleMode::Agriculture &&
		ImGui::CollapsingHeader("Agriculture Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		command.agricultureSettingsChanged |= ImGui::DragFloat3(
			"Emit Position", &command.agriculture.emitPosition.x, 0.1f);
		command.agricultureSettingsChanged |= ImGui::SliderFloat(
			"Particle Size", &command.agriculture.particleSize, 0.02f, 1.0f);
		command.agricultureSettingsChanged |= ImGui::SliderInt(
			"Count", &command.agriculture.particleCount, 1, 1024);
		if (ImGui::Button("Dirt Dust")) {
			command.emitAgricultureParticle = true;
			command.agricultureParticleType = editor::EditorAgricultureParticleType::DirtDust;
		}
		ImGui::SameLine();
		if (ImGui::Button("Water Splash")) {
			command.emitAgricultureParticle = true;
			command.agricultureParticleType = editor::EditorAgricultureParticleType::WaterSplash;
		}
		ImGui::SameLine();
		if (ImGui::Button("Harvest Sparkle")) {
			command.emitAgricultureParticle = true;
			command.agricultureParticleType = editor::EditorAgricultureParticleType::HarvestSparkle;
		}
		command.agricultureSettingsChanged |= ImGui::Checkbox(
			"Show Key Guide", &command.agriculture.showKeyGuide);
		if (command.agriculture.showKeyGuide) {
			ImGui::TextUnformatted("1: Dirt Dust  2: Water Splash  3: Harvest Sparkle");
			ImGui::TextUnformatted("4: Pollen / Spore  5: Bug Swarm");
		}
	} else if (command.gpuMode == editor::EditorGpuParticleMode::Interaction &&
		ImGui::CollapsingHeader("Interaction Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool resetRequired = false;
		resetRequired |= ImGui::SliderInt(
			"Particle Grid Count", &command.interaction.gridCount, 2, 10);
		resetRequired |= ImGui::SliderFloat(
			"Particle Size", &command.interaction.particleSize, 0.02f, 0.05f);
		command.interactionSettingsChanged |= resetRequired;
		command.interactionSettingsChanged |= ImGui::SliderFloat(
			"Brush Radius", &command.interaction.brushRadius, 0.1f, 10.0f);
		command.interactionSettingsChanged |= ImGui::SliderFloat(
			"Brush Strength", &command.interaction.brushStrength, 0.0f, 10.0f);
		command.interactionSettingsChanged |= ImGui::SliderFloat(
			"Interaction Damping", &command.interaction.damping, 0.80f, 0.995f);
		const uint32_t gridCount = static_cast<uint32_t>(
			std::clamp(command.interaction.gridCount, 2, 10));
		const uint32_t particleCount = gridCount * gridCount * gridCount;
		ImGui::Text("Current Particle Count: %u", particleCount);
		ImGui::Text(
			"Brush Position: %.2f, %.2f, %.2f",
			command.interaction.brushPosition.x,
			command.interaction.brushPosition.y,
			command.interaction.brushPosition.z);
		if (resetRequired) {
			command.resetInteractionGrid = true;
		}
		if (ImGui::Button("Reset Grid")) {
			command.resetInteractionGrid = true;
		}
		ImGui::TextUnformatted("Left Click on Game Viewport: Push");
		ImGui::TextUnformatted("Shift + Left Click: Pull");
	}

	command.alphaReferenceChanged |= ImGui::SliderFloat(
		"Alpha Discard", &command.alphaReference, 0.0f, 1.0f);
	ImGui::Separator();
	if (ImGui::Button("Clear All Particles")) {
		command.clearAll = true;
	}
	ImGui::End();
#else
	(void)viewData;
#endif
	return command;
}

void ParticleEffectWindow::SetOpen(bool open) noexcept {
	open_ = open;
}

bool ParticleEffectWindow::IsOpen() const noexcept {
	return open_;
}
