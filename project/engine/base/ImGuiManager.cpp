#include "ImGuiManager.h"

// シングルトンの実体取得
ImGuiManager* ImGuiManager::GetInstance() {
	static ImGuiManager instance;
	return &instance;
}

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager) {
#ifdef USE_IMGUI
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// CG4評価画面はメインウィンドウ内のDockSpaceだけを使用する。
	// Platform Windowを無効にして、評価パネルの分離と追加描画負荷を防ぐ。
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// 評価用UIの日本語を表示する。学校PCを含むWindows標準フォントを使い、
	// 読み込みに失敗した場合はImGuiの既定フォントへ安全にフォールバックする。
	if (!io.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/meiryo.ttc",
		17.0f,
		nullptr,
		io.Fonts->GetGlyphRangesJapanese())) {
		io.Fonts->AddFontDefault();
	}

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	srvIndex_ = srvManager_->Allocate();

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

	initInfo.SrvDescriptorHeap = srvManager_->GetSrvDescriptorHeap();

	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {
		ImGuiManager* manager = ImGuiManager::GetInstance();
		*out_cpu_handle = manager->srvManager_->GetCPUDescriptorHandle(manager->srvIndex_);
		*out_gpu_handle = manager->srvManager_->GetGPUDescriptorHandle(manager->srvIndex_);
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
