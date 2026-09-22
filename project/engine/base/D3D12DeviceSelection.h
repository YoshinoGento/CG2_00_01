#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <stdexcept>
#include <utility>

namespace graphics {
class InitializationError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline constexpr std::array<D3D_FEATURE_LEVEL, 3> kFeatureLevels{
    D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};

struct DeviceSelection {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    DXGI_ADAPTER_DESC1 adapter{};
    D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
    HRESULT error = DXGI_ERROR_UNSUPPORTED;
};

inline DeviceSelection TryHardwareAdapter(IDXGIAdapter1* adapter) {
    DeviceSelection result;
    if (!adapter) { result.error = E_POINTER; return result; }
    result.error = adapter->GetDesc1(&result.adapter);
    if (FAILED(result.error)) return result;
    if (result.adapter.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        result.error = DXGI_ERROR_UNSUPPORTED; return result;
    }
    for (const auto level : kFeatureLevels) {
        Microsoft::WRL::ComPtr<ID3D12Device> candidate;
        result.error = D3D12CreateDevice(adapter, level, IID_PPV_ARGS(&candidate));
        if (FAILED(result.error)) continue;
        D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_0};
        result.error = candidate->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &model, sizeof(model));
        if (FAILED(result.error) || model.HighestShaderModel < D3D_SHADER_MODEL_6_0) {
            if (SUCCEEDED(result.error)) result.error = DXGI_ERROR_UNSUPPORTED;
            return result;
        }
        result.device = std::move(candidate);
        result.featureLevel = level;
        result.error = S_OK;
        return result;
    }
    return result;
}

// Prefer the first capable hardware GPU in performance order; never silently use WARP.
inline DeviceSelection SelectHardwareDevice(IDXGIFactory6* factory) {
    DeviceSelection result;
    if (!factory) { result.error = E_POINTER; return result; }
    for (UINT index = 0; ; ++index) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        const HRESULT enumerated = factory->EnumAdapterByGpuPreference(index,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
        if (enumerated == DXGI_ERROR_NOT_FOUND) return result;
        if (FAILED(enumerated)) { result.error = enumerated; return result; }
        result = TryHardwareAdapter(adapter.Get());
        if (result.device) return result;
    }
}
}
