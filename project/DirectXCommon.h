#pragma once

#include "WinApp.h"

#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <chrono>

#include "externals/DirectXTex/DirectXTex.h"

class DirectXCommon {
public:
    // Initialize / Pre-draw / Post-draw
    void Initialize(WinApp* winApp);
    void PreDraw();
    void PostDraw();

    // Get SRV Handle
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

    // Get SRV Heap
    ID3D12DescriptorHeap* GetSrvHeap() const { return srvDescriptorHeap_.Get(); }

    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    // Allocate SRV index
    uint32_t AllocateSRVIndex();

    // Load Texture
    DirectX::ScratchImage LoadTexture(const std::string& filePath);

    // Compile Shader
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& profile);

    // Helper functions
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    // Create texture resource
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    // Upload texture data
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

    // Create Descriptor Heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

    // Max SRV count
    static const uint32_t kMaxSRVCount = 256;

private:
    void InitializeFixFPS();
    void UpdateFixFPS();

    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain(WinApp* winApp);
    void InitializeRenderTargetView();
    void InitializeDepthStencilView();
    void InitializeFence();

    // Initialize DXC
    void InitializeDXCCompiler();

    WinApp* winApp_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> backBuffers_;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    UINT rtvDescriptorSize_ = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
    UINT dsvDescriptorSize_ = 0;

    // SRV Heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    UINT srvDescriptorSize_ = 0;

    // Current SRV Index
    uint32_t currentSRVIndex_ = 0;

    // For FPS fixing
    std::chrono::steady_clock::time_point reference_;

    // DXC components
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};