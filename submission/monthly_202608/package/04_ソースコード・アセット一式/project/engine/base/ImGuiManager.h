#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// デバッグビルド時のみ ImGui を有効化するための切り替え
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

/// <summary>
/// ImGuiの初期化・更新・描画を一括管理するクラス
/// シングルトンパターンを採用し、どこからでも1つのインスタンスにアクセス可能
/// </summary>
class ImGuiManager {
public:
	// インスタンス取得用関数
	static ImGuiManager* GetInstance();

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="winApp">ウィンドウ管理クラスのポインタ</param>
	/// <param name="dxCommon">DirectX基盤クラスのポインタ</param>
	/// <param name="srvManager">SRV管理クラスのポインタ（ImGui用のSRVを確保するため）</param>
	void Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager);

	/// <summary>
	/// 終了処理（アプリ終了時に一度だけ呼ぶ）
	/// </summary>
	void Finalize();

	/// <summary>
	/// ImGuiのフレーム受付開始（メインループの最初の方で呼ぶ）
	/// </summary>
	void Begin();

	/// <summary>
	/// ImGuiのフレーム受付終了（UIの更新が終わった後に呼ぶ）
	/// </summary>
	void End();

	/// <summary>
	/// 画面への描画コマンド発行（描画処理の最後、PostDrawの直前に呼ぶ）
	/// </summary>
	void Draw();

	// Gameplay input queries this boundary instead of depending on ImGui types.
	[[nodiscard]] bool WantsCaptureKeyboard() const noexcept;
	[[nodiscard]] bool WantsCaptureMouse() const noexcept;
	[[nodiscard]] bool WantsTextInput() const noexcept;

	[[nodiscard]] std::size_t GetFontOptionCount() const noexcept;
	[[nodiscard]] const char* GetFontOptionName(std::size_t index) const noexcept;
	[[nodiscard]] std::size_t GetSelectedFontIndex() const noexcept;
	bool SelectFont(std::size_t index);

private:
	// コンストラクタを private にして外部からの生成を禁止（シングルトン）
	ImGuiManager() = default;
	~ImGuiManager() = default;
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

#ifdef USE_IMGUI
	// 各種ポインタの保持
	static void AllocateSrvDescriptor(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);
	static void FreeSrvDescriptor(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
	void ConfigureFonts(ImGuiIO& io);
	bool TryAddFontOption(
		ImGuiIO& io,
		const char* displayName,
		const char* fontPath,
		float pixelSize);

	struct FontOption {
		std::string displayName;
		ImFont* font = nullptr;
	};

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// ImGuiが使用するディスクリプタ（SRV）のインデックス
	std::unordered_map<SIZE_T, uint32_t> srvDescriptorIndices_;
	std::vector<FontOption> fontOptions_;
	std::size_t selectedFontIndex_ = 0;
#endif
};
