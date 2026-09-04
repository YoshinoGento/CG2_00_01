#include "debug/Engine2DFeatureDebugWindow.h"

#include "2d/TextureManager.h"
#include "externals/imgui/imgui.h"

namespace {
	void DrawFeatureLine(const char* label, const char* value)
	{
		ImGui::BulletText("%s: %s", label, value);
	}

	void DrawSpriteSection()
	{
		if (!ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		DrawFeatureLine("Sprite::SetTextureRect", "Implemented");
		DrawFeatureLine("TextureRect unit", "pixel");
		DrawFeatureLine("Verified image", "Resources/uvChecker.png");
		ImGui::BulletText("Supports full texture default display");
		ImGui::BulletText("Supports clamp for invalid rect");
	}

	void DrawTextureManagerSection()
	{
		if (!ImGui::CollapsingHeader("TextureManager", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		const TextureManager* textureManager = TextureManager::GetInstance();
		const TextureManager::Statistics statistics = textureManager->GetStatistics();
		DrawFeatureLine("Lifecycle", textureManager->IsInitialized() ? "Initialized" : "Finalized");
		ImGui::Text("Texture2D resources: %zu", statistics.texture2DCount);
		ImGui::Text("TextureCube resources: %zu", statistics.textureCubeCount);
		ImGui::Text("Estimated GPU memory: %.2f MiB",
			static_cast<double>(statistics.estimatedGpuBytes) / (1024.0 * 1024.0));
		DrawFeatureLine("Duplicate paths", "cached");
		DrawFeatureLine("Missing 2D asset", "magenta checker fallback");
		DrawFeatureLine("Upload synchronization", "dedicated command list + fence");
	}

	void DrawJsonFileSection()
	{
		if (!ImGui::CollapsingHeader("JsonFile", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		DrawFeatureLine("JsonFile", "Implemented");
		ImGui::BulletText("Exists / Load / Save");
		DrawFeatureLine("Library", "nlohmann/json");
		DrawFeatureLine("Error log", "Logger::Log()");
	}

	void DrawBitmapFontSection()
	{
		if (!ImGui::CollapsingHeader("BitmapFont", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		DrawFeatureLine("BitmapFont", "Implemented");
		DrawFeatureLine("JSON config", "Resources/ui/font/ascii_bitmap_font.json");
		DrawFeatureLine("Texture", "Resources/ui/font/ascii_bitmap_font.png");
		DrawFeatureLine("GlyphSize", "32 x 32");
		DrawFeatureLine("Columns", "16");
		DrawFeatureLine("Characters", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz :/-+.!%$");
		DrawFeatureLine("InitializeFromJson", "Implemented");
	}

	void DrawSpriteTextSection()
	{
		if (!ImGui::CollapsingHeader("SpriteText", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		DrawFeatureLine("SpriteText", "Implemented");
		ImGui::BulletText("Uses external BitmapFont reference");
		DrawFeatureLine("SetText", "Supported");
		DrawFeatureLine("SetPosition", "Supported");
		DrawFeatureLine("SetScale", "Supported");
		DrawFeatureLine("SetColor", "Supported");
		DrawFeatureLine("SetCharacterSpacing", "Supported");
		DrawFeatureLine("Unsupported characters", "skipped safely");
		DrawFeatureLine("Per-frame allocation", "avoided");
	}

	void DrawAnimatedSpriteSection()
	{
		if (!ImGui::CollapsingHeader("AnimatedSprite", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		DrawFeatureLine("AnimatedSprite", "Implemented");
		ImGui::BulletText("Uses Sprite::SetTextureRect");
		DrawFeatureLine("Play / Stop / Reset", "Supported");
		DrawFeatureLine("Loop true / false", "Supported");
		DrawFeatureLine("Update(deltaTime)", "Supported");
		ImGui::BulletText("Frame rect formula:");
		ImGui::Indent();
		ImGui::TextUnformatted("x = (frame % columns) * frameWidth");
		ImGui::TextUnformatted("y = (frame / columns) * frameHeight");
		ImGui::Unindent();
		DrawFeatureLine("Per-frame allocation", "avoided");
	}

	void DrawNextStepsSection()
	{
		if (!ImGui::CollapsingHeader("Next Steps", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		ImGui::BulletText("Optional: AnimatedSprite JSON config");
		ImGui::BulletText("Optional: SpriteText unsupported character fallback");
		ImGui::BulletText("Optional: formal font license check");
		ImGui::BulletText("Next major step: FarmHUD design, without putting HUD logic into GamePlayScene");
	}
}

void Engine2DFeatureDebugWindow::Draw()
{
	if (ImGui::Begin("2D Feature Debug")) {
		DrawSpriteSection();
		DrawTextureManagerSection();
		DrawJsonFileSection();
		DrawBitmapFontSection();
		DrawSpriteTextSection();
		DrawAnimatedSpriteSection();
		DrawNextStepsSection();
	}
	ImGui::End();
}
