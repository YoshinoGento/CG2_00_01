#include "Game.h"
#include "debug/ResourceLeakChecker.h"
#include <Windows.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace {

constexpr wchar_t kRuntimeResourceProbe[] =
	L"Resources\\shader\\PostProcess.VS.hlsl";
constexpr std::size_t kMaximumParentSearchDepth = 8;

bool TrySetRuntimeRoot(const std::filesystem::path& candidate)
{
	if (candidate.empty()) {
		return false;
	}

	std::error_code error;
	if (!std::filesystem::is_regular_file(candidate / kRuntimeResourceProbe, error) || error) {
		return false;
	}

	const std::filesystem::path normalized =
		std::filesystem::weakly_canonical(candidate, error);
	if (error || normalized.empty()) {
		return false;
	}
	return SetCurrentDirectoryW(normalized.c_str()) != FALSE;
}

bool ConfigureRuntimeWorkingDirectory()
{
	std::error_code error;
	const std::filesystem::path currentDirectory =
		std::filesystem::current_path(error);
	if (!error &&
		(TrySetRuntimeRoot(currentDirectory) ||
		 TrySetRuntimeRoot(currentDirectory / L"project"))) {
		return true;
	}

	// Runtime paths are resolved once before engine threads or file systems start.
	std::array<wchar_t, 32768> executablePathBuffer{};
	const DWORD pathLength = GetModuleFileNameW(
		nullptr,
		executablePathBuffer.data(),
		static_cast<DWORD>(executablePathBuffer.size()));
	if (pathLength == 0 || pathLength >= executablePathBuffer.size()) {
		return false;
	}

	std::filesystem::path directory = std::filesystem::path(
		std::wstring(executablePathBuffer.data(), pathLength)).parent_path();
	for (std::size_t depth = 0;
		 depth < kMaximumParentSearchDepth && !directory.empty();
		 ++depth) {
		if (TrySetRuntimeRoot(directory) ||
			TrySetRuntimeRoot(directory / L"project")) {
			return true;
		}

		const std::filesystem::path parent = directory.parent_path();
		if (parent == directory) {
			break;
		}
		directory = parent;
	}
	return false;
}

} // namespace

/**
 * WinMain: プログラムが最初に動き出す場所
 */
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	if (!ConfigureRuntimeWorkingDirectory()) {
		MessageBoxW(
			nullptr,
			L"起動に必要な Resources フォルダを見つけられませんでした。\n"
			L"リポジトリ構成を保ったまま再ビルドしてください。",
			L"CG2 起動エラー",
			MB_OK | MB_ICONERROR);
		return EXIT_FAILURE;
	}

	// 1. ゲーム本体（Gameクラス）を作成します。
	// Framework（エンジン層）のポインタで持つことで、共通の Run() 関数を呼び出せるようにしています。
#ifdef _DEBUG
	ResourceLeakChecker resourceLeakChecker;
#endif
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	// 2. ゲームを実行します。
	// この1行の中で「初期化 → ループ（更新・描画） → 終了処理」が自動的に行われます。
	game->Run();
	// Release the complete engine before asking DXGI for live objects. Reporting
	// from a process-termination callback is too late and produces a misleading
	// untyped "simple reporting" dump.
	game.reset();
#ifdef _DEBUG
	resourceLeakChecker.ReportLiveObjects();
#endif

	// 3. プログラムを正常終了します。
	return 0;
}
