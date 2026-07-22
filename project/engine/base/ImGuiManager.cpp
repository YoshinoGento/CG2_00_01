#include "ImGuiManager.h"
#include "Logger.h"

#ifdef USE_IMGUI
#include <array>
#include <filesystem>

namespace {
constexpr float kEditorFontSize = 18.0f;
constexpr std::array<const char*, 2> kNotoSansJpPaths = {
	"Resources/fonts/NotoSansJP-VF.ttf",
	"C:/Windows/Fonts/NotoSansJP-VF.ttf",
};

const char* FindFirstExistingPath(const std::array<const char*, 2>& paths) {
	for (const char* path : paths) {
		std::error_code error;
		if (std::filesystem::exists(path, error) && !error) {
			return path;
		}
	}
	return nullptr;
}
} // namespace
#endif

#ifdef USE_IMGUI
void ImGuiManager::AllocateSrvDescriptor(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
	auto* manager = info ? static_cast<ImGuiManager*>(info->UserData) : nullptr;
	if (manager == nullptr || manager->srvManager_ == nullptr || outCpuHandle == nullptr || outGpuHandle == nullptr) {
		Logger::Log("ImGui SRV allocation rejected invalid input.");
		return;
	}

	const uint32_t index = manager->srvManager_->Allocate();
	if (index == SrvManager::kInvalidIndex) {
		*outCpuHandle = {};
		*outGpuHandle = {};
		return;
	}

	*outCpuHandle = manager->srvManager_->GetCPUDescriptorHandle(index);
	*outGpuHandle = manager->srvManager_->GetGPUDescriptorHandle(index);
	manager->srvDescriptorIndices_.emplace(outCpuHandle->ptr, index);
}

void ImGuiManager::FreeSrvDescriptor(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	[[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
	auto* manager = info ? static_cast<ImGuiManager*>(info->UserData) : nullptr;
	if (manager == nullptr || manager->srvManager_ == nullptr) {
		Logger::Log("ImGui SRV release rejected invalid input.");
		return;
	}

	const auto descriptor = manager->srvDescriptorIndices_.find(cpuHandle.ptr);
	if (descriptor == manager->srvDescriptorIndices_.end()) {
		Logger::Log("ImGui SRV release rejected an unknown descriptor.");
		return;
	}

	if (manager->srvManager_->IsAllocated(descriptor->second)) {
		manager->srvManager_->Release(descriptor->second);
	}
	manager->srvDescriptorIndices_.erase(descriptor);
}
#endif

#ifdef USE_IMGUI
bool ImGuiManager::TryAddFontOption(
	ImGuiIO& io,
	const char* displayName,
	const char* fontPath,
	float pixelSize) {
	if (displayName == nullptr || fontPath == nullptr || pixelSize <= 0.0f) {
		return false;
	}

	std::error_code error;
	if (!std::filesystem::exists(fontPath, error) || error) {
		return false;
	}

	// ImGui 1.92 loads glyphs on demand, so full CJK ranges are not pre-baked.
	ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, pixelSize);
	if (font == nullptr) {
		Logger::Log(std::string("ImGui font registration failed: ") + fontPath);
		return false;
	}

	fontOptions_.push_back({ displayName, font });
	Logger::Log(std::string("ImGui font registered: ") + displayName + " <- " + fontPath);
	return true;
}

void ImGuiManager::ConfigureFonts(ImGuiIO& io) {
	fontOptions_.clear();
	selectedFontIndex_ = 0;

	if (const char* notoSansJpPath = FindFirstExistingPath(kNotoSansJpPaths)) {
		TryAddFontOption(io, "Noto Sans JP", notoSansJpPath, kEditorFontSize);
	}
	TryAddFontOption(io, "Yu Gothic", "C:/Windows/Fonts/YuGothM.ttc", kEditorFontSize);
	TryAddFontOption(io, "Meiryo", "C:/Windows/Fonts/meiryo.ttc", kEditorFontSize);

	if (fontOptions_.empty()) {
		if (ImFont* builtInFont = io.Fonts->AddFontDefault()) {
			fontOptions_.push_back({ "ImGui Default (Japanese unsupported)", builtInFont });
		}
		Logger::Log("ImGui Japanese fonts were not found. Using the built-in font.");
	}

	if (!fontOptions_.empty()) {
		io.FontDefault = fontOptions_.front().font;
		Logger::Log(std::string("ImGui default font: ") + fontOptions_.front().displayName);
	}
}
#endif

// シングルトンの実体取得
ImGuiManager* ImGuiManager::GetInstance() {
	static ImGuiManager instance;
	return &instance;
}

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager) {
#ifdef USE_IMGUI
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	srvDescriptorIndices_.clear();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// ドッキングとマルチビューポートを有効化
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ConfigureFonts(io);

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	// --- 初期化設定の修正 ---
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = dxCommon_->GetDevice();
	initInfo.CommandQueue = dxCommon_->GetCommandQueue();
	initInfo.NumFramesInFlight = (uint32_t)dxCommon_->GetSwapChainResourcesNum();

	// ★【重要修正】エラーログに合わせて _SRGB を取ります
	// ログにて render target format = R8G8B8A8_UNORM と出ていたためこちらに合わせます
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// DSV(深度)もエンジンの設定に合わせます。
	// もしエンジンの DirectXCommon で D32_FLOAT を使っているなら D32_FLOAT にしてください。
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	initInfo.UserData = this;
	initInfo.SrvDescriptorHeap = srvManager_->GetSrvDescriptorHeap();

	initInfo.SrvDescriptorAllocFn = &ImGuiManager::AllocateSrvDescriptor;
	initInfo.SrvDescriptorFreeFn = &ImGuiManager::FreeSrvDescriptor;

	ImGui_ImplDX12_Init(&initInfo);
	if (!ImGui::GetIO().Fonts->Build()) {
		Logger::Log("ImGui font atlas build failed.");
	}
#endif
}

void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	for (const auto& [handle, index] : srvDescriptorIndices_) {
		(void)handle;
		if (srvManager_ != nullptr && srvManager_->IsAllocated(index)) {
			srvManager_->Release(index);
		}
	}
	srvDescriptorIndices_.clear();
	fontOptions_.clear();
	selectedFontIndex_ = 0;
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
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
		ImGui::RenderPlatformWindowsDefault(nullptr, (void*)commandList);
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

bool ImGuiManager::WantsTextInput() const noexcept {
#ifdef USE_IMGUI
	return ImGui::GetCurrentContext() &&
		(ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive());
#else
	return false;
#endif
}

std::size_t ImGuiManager::GetFontOptionCount() const noexcept {
#ifdef USE_IMGUI
	return fontOptions_.size();
#else
	return 0;
#endif
}

const char* ImGuiManager::GetFontOptionName(std::size_t index) const noexcept {
#ifdef USE_IMGUI
	if (index < fontOptions_.size()) {
		return fontOptions_[index].displayName.c_str();
	}
#else
	(void)index;
#endif
	return "";
}

std::size_t ImGuiManager::GetSelectedFontIndex() const noexcept {
#ifdef USE_IMGUI
	return selectedFontIndex_;
#else
	return 0;
#endif
}

bool ImGuiManager::SelectFont(std::size_t index) {
#ifdef USE_IMGUI
	if (index >= fontOptions_.size() || fontOptions_[index].font == nullptr ||
		ImGui::GetCurrentContext() == nullptr) {
		return false;
	}

	selectedFontIndex_ = index;
	ImGui::GetIO().FontDefault = fontOptions_[index].font;
	Logger::Log(std::string("ImGui font selected: ") + fontOptions_[index].displayName);
	return true;
#else
	(void)index;
	return false;
#endif
}
