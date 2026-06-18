#include "WinApp.h"
#include "audio/Audio.h"
#pragma comment(lib, "winmm.lib")

// ★ImGui用ウィンドウプロシージャの extern 宣言
#ifdef USE_IMGUI
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ★ImGuiにメッセージを転送し、ImGuiが処理した場合はゲーム側の処理をスキップ
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ...Initialize, ProcessMessage 等は変更なし...
void WinApp::Initialize() {
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	timeBeginPeriod(1);
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	hwnd = CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top, nullptr, nullptr, wc.hInstance, nullptr);
	ShowWindow(hwnd, SW_SHOW);
}

void WinApp::Update() {}

void WinApp::Finalize() {
	CloseWindow(hwnd);
	CoUninitialize();
}

void WinApp::ToggleBorderlessFullscreen() {
	SetBorderlessFullscreen(!isBorderlessFullscreen_);
}

void WinApp::SetBorderlessFullscreen(bool enabled) {
	if (!hwnd || isBorderlessFullscreen_ == enabled) {
		return;
	}

	if (enabled) {
		windowedStyle_ = GetWindowLongPtr(hwnd, GWL_STYLE);
		windowedExStyle_ = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		GetWindowRect(hwnd, &windowedRect_);

		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);
		HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		if (!GetMonitorInfo(monitor, &monitorInfo)) {
			return;
		}

		SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowLongPtr(
			hwnd,
			GWL_EXSTYLE,
			windowedExStyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
		SetWindowPos(
			hwnd,
			HWND_TOP,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		isBorderlessFullscreen_ = true;
	} else {
		SetWindowLongPtr(hwnd, GWL_STYLE, windowedStyle_);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, windowedExStyle_);
		SetWindowPos(
			hwnd,
			nullptr,
			windowedRect_.left,
			windowedRect_.top,
			windowedRect_.right - windowedRect_.left,
			windowedRect_.bottom - windowedRect_.top,
			SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		isBorderlessFullscreen_ = false;
	}
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (msg.message == WM_QUIT) {
		return true;
	}
	return false;
}
