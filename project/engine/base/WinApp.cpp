#include "WinApp.h"
#include "audio/Audio.h"
#include <algorithm>
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
	case WM_KEYDOWN:
		if (wparam == VK_F11) {
			if (WinApp* winApp = reinterpret_cast<WinApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				winApp->ToggleBorderlessFullscreen();
				return 0;
			}
		}
		break;
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
	RECT wrc = { 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	hwnd = CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top, nullptr, nullptr, wc.hInstance, nullptr);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
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

uint32_t WinApp::GetClientWidth() const {
	if (!hwnd) {
		return WinApp::kClientWidth;
	}

	RECT clientRect{};
	if (!GetClientRect(hwnd, &clientRect)) {
		return WinApp::kClientWidth;
	}

	return static_cast<uint32_t>((std::max)(clientRect.right - clientRect.left, 0L));
}

uint32_t WinApp::GetClientHeight() const {
	if (!hwnd) {
		return WinApp::kClientHeight;
	}

	RECT clientRect{};
	if (!GetClientRect(hwnd, &clientRect)) {
		return WinApp::kClientHeight;
	}

	return static_cast<uint32_t>((std::max)(clientRect.bottom - clientRect.top, 0L));
}

void WinApp::ToggleBorderlessFullscreen() {
	if (!hwnd) {
		return;
	}

	if (!isBorderlessFullscreen_) {
		windowedStyle_ = static_cast<DWORD>(GetWindowLong(hwnd, GWL_STYLE));
		GetWindowRect(hwnd, &windowedRect_);

		HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (!GetMonitorInfo(monitor, &monitorInfo)) {
			return;
		}

		SetWindowLong(hwnd, GWL_STYLE, static_cast<LONG>(windowedStyle_ & ~WS_OVERLAPPEDWINDOW));
		SetWindowPos(
			hwnd,
			HWND_TOP,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		isBorderlessFullscreen_ = true;
		return;
	}

	SetWindowLong(hwnd, GWL_STYLE, static_cast<LONG>(windowedStyle_));
	SetWindowPos(
		hwnd,
		nullptr,
		windowedRect_.left,
		windowedRect_.top,
		windowedRect_.right - windowedRect_.left,
		windowedRect_.bottom - windowedRect_.top,
		SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	isBorderlessFullscreen_ = false;
}
