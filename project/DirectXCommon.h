#pragma once

#include "WinApp.h"

#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h> // シェーダーコンパイラ用
#include <wrl.h>
#include <string>
#include <chrono>

#include "externals/DirectXTex/DirectXTex.h"

class DirectXCommon {
public:
    // 初期化 / 描画前後
    void Initialize(WinApp* winApp);
    void PreDraw();
    void PostDraw();

    // SRV ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

    // SRVヒープの取得
    ID3D12DescriptorHeap* GetSrvHeap() const { return srvDescriptorHeap_.Get(); }

    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    // ★追加: SRVの空き番号を確保する関数
    uint32_t AllocateSRVIndex();

    // テクスチャ読み込み関数
    DirectX::ScratchImage LoadTexture(const std::string& filePath);

    // ★追加: シェーダーコンパイル関数
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& profile);

    // ===== ヘルパー関数 =====
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    // テクスチャリソースの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    // テクスチャデータの転送
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

    // デスクリプタヒープ生成関数
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

    // 最大SRV数
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

    // ★追加: DXCコンパイラの初期化
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

    // SRV 用ヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    UINT srvDescriptorSize_ = 0;

    // 現在使用しているSRVインデックス
    uint32_t currentSRVIndex_ = 0;

    // FPS固定用変数
    std::chrono::steady_clock::time_point reference_;

    // ★追加: DXC関連
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};