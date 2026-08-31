#pragma once
#include <wrl.h>
#include <Windows.h>
#include <stdint.h>


using namespace Microsoft::WRL;

// WindowsAPI
class WinApp {
public:// 静的メンバー関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	// 初期化
	void Initialize();

	//更新
	void Update();

	// 終了
	void Finalize();

#ifdef _DEBUG
	static const int32_t kClientWidth = 1600;
	static const int32_t kClientHeight = 900;
#else
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;
#endif
	// ウィンドウサイズを表す構造体体にクライアント領域を入れる
	RECT wrc = { 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };

	//　メッセージの処理
	bool ProcessMessage();

public:
	// Getter
	HWND GetHwnd() const { return hwnd; }
	uint32_t GetClientWidth() const;
	uint32_t GetClientHeight() const;

	// getter
	HINSTANCE GetHInstance() const { return wc.hInstance; }
	void ToggleBorderlessFullscreen();
private:

	HWND hwnd= nullptr;  // ← ウィンドウハンドルを保持するメンバ変数

	WNDCLASS wc{};
	bool isBorderlessFullscreen_ = false;
	DWORD windowedStyle_ = WS_OVERLAPPEDWINDOW;
	RECT windowedRect_ = {};
};
