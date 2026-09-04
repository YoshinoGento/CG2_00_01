#include "ResourceLeakChecker.h"

#ifdef _DEBUG
#include <dxgi1_3.h>
#include <dxgidebug.h>
#include <wrl.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
#endif

ResourceLeakChecker::~ResourceLeakChecker() {
	ReportLiveObjects();
}

void ResourceLeakChecker::ReportLiveObjects() {
	if (reported_) {
		return;
	}
	reported_ = true;
#ifdef _DEBUG
	ComPtr<IDXGIDebug1> debugController;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugController)))) {
		debugController->ReportLiveObjects(
			DXGI_DEBUG_ALL,
			static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	}
#endif
}
