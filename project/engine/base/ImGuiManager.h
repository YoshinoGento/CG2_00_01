#pragma once

#include "DirectXCommon.h"
#include "SrvManager.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <vector>
#endif

class ImGuiManager {
public:
	static ImGuiManager* GetInstance();

	void Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager);
	void Finalize();

	void Begin();
	void End();
	void Draw();

	// Gameplay input queries this boundary instead of depending on ImGui types.
	[[nodiscard]] bool WantsCaptureKeyboard() const noexcept;
	[[nodiscard]] bool WantsCaptureMouse() const noexcept;

	[[nodiscard]] int GetDebugFontCount() const noexcept;
	[[nodiscard]] int GetCurrentDebugFontIndex() const noexcept;
	[[nodiscard]] const char* GetDebugFontName(int index) const noexcept;
	void SetCurrentDebugFontIndex(int index) noexcept;
	bool SetCurrentDebugFontByName(const char* name) noexcept;

private:
	ImGuiManager() = default;
	~ImGuiManager() = default;
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

#ifdef USE_IMGUI
	struct DebugFontEntry {
		const char* name = "";
		ImFont* font = nullptr;
	};

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	uint32_t srvIndex_ = 0;

	std::vector<DebugFontEntry> debugFonts_;
	int currentDebugFontIndex_ = 0;
	bool debugFontPushedThisFrame_ = false;
#endif
};
