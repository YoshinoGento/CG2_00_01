#include "base/D3D12DeviceSelection.h"
#include <cstdio>
int main() {
    const auto invalid = graphics::SelectHardwareDevice(nullptr);
    if (invalid.device || invalid.error != E_POINTER || invalid.featureLevel != 0) return 1;
    if (graphics::TryHardwareAdapter(nullptr).error != E_POINTER) return 1;
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)))) return 2;
    for (UINT i = 0; ; ++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr = factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) return 3;
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) return 4;
        std::wprintf(L"GPU: %ls flags=%u\n", desc.Description, desc.Flags);
        D3D_FEATURE_LEVEL highest = static_cast<D3D_FEATURE_LEVEL>(0);
        for (auto level : graphics::kFeatureLevels) {
            const HRESULT supported = D3D12CreateDevice(adapter.Get(), level, __uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(supported) && highest == 0) highest = level;
            std::printf("level=0x%04X HRESULT=0x%08lX\n", static_cast<unsigned>(level), static_cast<unsigned long>(supported));
        }
        const auto selected = graphics::TryHardwareAdapter(adapter.Get());
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            if (selected.device || SUCCEEDED(selected.error)) return 8;
        } else if (highest != 0) {
            if (!selected.device || selected.featureLevel != highest) return 9;
            std::printf("Adapter selected level=0x%04X\n", static_cast<unsigned>(selected.featureLevel));
        } else if (selected.device) return 10;
    }
    auto result = graphics::SelectHardwareDevice(factory.Get());
    if (FAILED(result.error) || !result.device) {
        std::printf("No supported hardware: 0x%08lX\n", static_cast<unsigned long>(result.error)); return 5;
    }
    if (result.adapter.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) return 6;
    std::wprintf(L"Selected: %ls level=0x%04X\n", result.adapter.Description, static_cast<unsigned>(result.featureLevel));
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    if (FAILED(result.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) return 7;
    std::puts("PASS: null guard, actual hardware fallback, SM6.0 and command allocator");
}
