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

/**
 * DirectXCommonクラス
 * DirectX12の基盤管理に加え、エディタ用の「レンダーテクスチャ」管理機能を追加。
 */
class DirectXCommon {
public:
    void Initialize(WinApp* winApp);

    // --- 描画フロー管理 ---
    // 1. ゲーム画面（テクスチャ）への描画を開始する
    void PreDraw();
    // 2. 描画先を「実際のモニター（スワップチェーン）」に切り替える（ImGui描画用）
    void PreDrawToSwapChain(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle);
    void RestoreRenderTextureToRenderTarget();
    // 3. 全ての描画を終了し、画面を表示する
    void PostDraw();

    // --- ゲッター ---
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
    size_t GetSwapChainResourcesNum() const { return backBuffers_.size(); }

    // ImGuiに渡すための、描き込み済みテクスチャリソースを取得
    ID3D12Resource* GetRenderTextureResource() const { return renderTextureResource_.Get(); }

    // --- 各種ヘルパー ---
    DirectX::ScratchImage LoadTexture(const std::string& filePath);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& profile);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(size_t sizeInBytes, D3D12_RESOURCE_STATES initialState);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const float* clearColor);
    uint32_t AllocateRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index) const;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);

private:
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain(WinApp* winApp);
    void InitializeRenderTargetView(); // RTVヒープのサイズを拡張します
    void InitializeDepthStencilView();
    void InitializeRenderTexture();    // ゲーム画面用のテクスチャを作成
    void InitializeCopyImagePipeline();
    void InitializeFence();
    void InitializeDXCCompiler();
    void InitializeFixFPS();
    void UpdateFixFPS();
    void TransitionRenderTexture(D3D12_RESOURCE_STATES stateAfter);
    void DrawRenderTextureToSwapChain(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle);

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

    // --- 追加：ゲーム画面を保存するテクスチャ ---
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    D3D12_RESOURCE_STATES renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // RTVヒープ内での場所（0,1はモニター用、2はゲーム画面用）
    const UINT kRenderTextureRTVIndex = 2;
    uint32_t nextRtvIndex_ = 3; // RTVの次の割り当てインデックス

    static constexpr DXGI_FORMAT kRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    float renderTextureClearColor_[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

    Microsoft::WRL::ComPtr<ID3D12RootSignature> copyImageRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyImagePipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = {};
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
    std::chrono::steady_clock::time_point reference_;
};
