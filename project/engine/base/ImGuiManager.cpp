#include "ImGuiManager.h"

#include "base/Logger.h"

#ifdef USE_IMGUI
#include <filesystem>
#include <string>
#include <cstring>
#endif

ImGuiManager* ImGuiManager::GetInstance() {
	static ImGuiManager instance;
	return &instance;
}

#ifdef USE_IMGUI
namespace {
const char* FindJapaneseFontPath() {
	static constexpr const char* kFontPaths[] = {
		"Resources/fonts/NotoSansJP-VF.ttf",
		"C:/Windows/Fonts/NotoSansJP-VF.ttf",
		"C:/Windows/Fonts/meiryo.ttc",
		"C:/Windows/Fonts/YuGothM.ttc",
		"C:/Windows/Fonts/msgothic.ttc",
	};

	for (const char* path : kFontPaths) {
		std::error_code error;
		if (std::filesystem::exists(path, error)) {
			return path;
		}
	}

	return nullptr;
}

ImFont* LoadDebugFont(ImGuiIO& io, const char* name, const char* path, float size) {
	ImFont* font = io.Fonts->AddFontFromFileTTF(path, size, nullptr, io.Fonts->GetGlyphRangesJapanese());
	if (!font) {
		Logger::Log(std::string("ImGuiManager: failed to load debug font: ") + name + " from " + path);
		return nullptr;
	}

	Logger::Log(std::string("ImGuiManager: loaded debug font: ") + name + " from " + path);
	return font;
}
}
#endif

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager) {
#ifdef USE_IMGUI
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	debugFonts_.clear();
	currentDebugFontIndex_ = 0;
	debugFontPushedThisFrame_ = false;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	ImFont* defaultFont = io.Fonts->AddFontDefault();
	debugFonts_.push_back({ "Default", defaultFont });
	io.FontDefault = defaultFont;

	if (const char* japaneseFontPath = FindJapaneseFontPath()) {
		const int firstJapaneseFontIndex = static_cast<int>(debugFonts_.size());
		if (ImFont* font = LoadDebugFont(io, "Japanese 16", japaneseFontPath, 16.0f)) {
			debugFonts_.push_back({ "Japanese 16", font });
		}
		if (ImFont* font = LoadDebugFont(io, "Japanese 18", japaneseFontPath, 18.0f)) {
			debugFonts_.push_back({ "Japanese 18", font });
		}
		if (ImFont* font = LoadDebugFont(io, "Japanese 20", japaneseFontPath, 20.0f)) {
			debugFonts_.push_back({ "Japanese 20", font });
		}

		if (static_cast<int>(debugFonts_.size()) > firstJapaneseFontIndex) {
			currentDebugFontIndex_ = firstJapaneseFontIndex;
			io.FontDefault = debugFonts_[currentDebugFontIndex_].font;
		}
	} else {
		Logger::Log("ImGuiManager: Japanese-capable font was not found. Using default ImGui font.");
	}

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	srvIndex_ = srvManager_->Allocate();

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = dxCommon_->GetDevice();
	initInfo.CommandQueue = dxCommon_->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<uint32_t>(dxCommon_->GetSwapChainResourcesNum());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.SrvDescriptorHeap = srvManager_->GetSrvDescriptorHeap();
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
		ImGuiManager* manager = ImGuiManager::GetInstance();
		*outCpuHandle = manager->srvManager_->GetCPUDescriptorHandle(manager->srvIndex_);
		*outGpuHandle = manager->srvManager_->GetGPUDescriptorHandle(manager->srvIndex_);
	};
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};

	ImGui_ImplDX12_Init(&initInfo);
	ImGui::GetIO().Fonts->Build();
#endif
}

void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	debugFonts_.clear();
	currentDebugFontIndex_ = 0;
	debugFontPushedThisFrame_ = false;
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	debugFontPushedThisFrame_ = false;
	if (currentDebugFontIndex_ >= 0 && currentDebugFontIndex_ < static_cast<int>(debugFonts_.size())) {
		ImFont* font = debugFonts_[currentDebugFontIndex_].font;
		if (font) {
			ImGui::PushFont(font);
			debugFontPushedThisFrame_ = true;
		}
	}
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
	if (debugFontPushedThisFrame_) {
		ImGui::PopFont();
		debugFontPushedThisFrame_ = false;
	}
	ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef USE_IMGUI
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	ID3D12DescriptorHeap* ppHeaps[] = { srvManager_->GetSrvDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, static_cast<void*>(commandList));
	}
#endif
}

bool ImGuiManager::WantsCaptureKeyboard() const noexcept {
#ifdef USE_IMGUI
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
#else
	return false;
#endif
}

bool ImGuiManager::WantsCaptureMouse() const noexcept {
#ifdef USE_IMGUI
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
#else
	return false;
#endif
}

int ImGuiManager::GetDebugFontCount() const noexcept {
#ifdef USE_IMGUI
	return static_cast<int>(debugFonts_.size());
#else
	return 0;
#endif
}

int ImGuiManager::GetCurrentDebugFontIndex() const noexcept {
#ifdef USE_IMGUI
	return currentDebugFontIndex_;
#else
	return 0;
#endif
}

const char* ImGuiManager::GetDebugFontName(int index) const noexcept {
#ifdef USE_IMGUI
	if (index < 0 || index >= static_cast<int>(debugFonts_.size())) {
		return "Unknown";
	}

	return debugFonts_[index].name;
#else
	(void)index;
	return "Unavailable";
#endif
}

void ImGuiManager::SetCurrentDebugFontIndex(int index) noexcept {
#ifdef USE_IMGUI
	if (index < 0 || index >= static_cast<int>(debugFonts_.size())) {
		return;
	}

	currentDebugFontIndex_ = index;
#else
	(void)index;
#endif
}

bool ImGuiManager::SetCurrentDebugFontByName(const char* name) noexcept {
#ifdef USE_IMGUI
	if (!name) {
		return false;
	}

	for (int index = 0; index < static_cast<int>(debugFonts_.size()); ++index) {
		if (debugFonts_[index].name && std::strcmp(debugFonts_[index].name, name) == 0) {
			currentDebugFontIndex_ = index;
			return true;
		}
	}

	return false;
#else
	(void)name;
	return false;
#endif
}
