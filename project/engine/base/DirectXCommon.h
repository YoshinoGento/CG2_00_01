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
    enum class FullscreenPostEffectType {
        Copy = 0,
        Grayscale,
        Sepia,
        Blur,
        Bloom,
        Count,
    };

    struct FullscreenPostEffectParameter {
        float grayscaleIntensity = 1.0f;
        float sepiaIntensity = 1.0f;
        float blurStrength = 1.0f;
        float padding = 0.0f;
        float bloomThreshold = 0.65f;
        float bloomIntensity = 1.0f;
        float bloomRadius = 6.0f;
        float bloomSoftKnee = 0.2f;
    };
    static_assert(sizeof(FullscreenPostEffectParameter) == 32);

    void Initialize(WinApp* winApp);

    // --- 描画フロー管理 ---
    // 1. ゲーム画面（テクスチャ）への描画を開始する
    void PreDraw();
    // 2. 描画先を「実際のモニター（スワップチェーン）」に切り替える（ImGui描画用）
    void PreDrawToSwapChain(
        D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE postEffectResultSrvHandle,
        FullscreenPostEffectType postEffectType = FullscreenPostEffectType::Copy);
    void RestoreRenderTextureToRenderTarget();
    void SetFullscreenPostEffectParameter(const FullscreenPostEffectParameter& parameter);
    // 3. 全ての描画を終了し、画面を表示する
    void PostDraw();

    // --- ゲッター ---
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
    size_t GetSwapChainResourcesNum() const { return backBuffers_.size(); }

    // ImGuiに渡すための、描き込み済みテクスチャリソースを取得
    ID3D12Resource* GetRenderTextureResource() const { return renderTextureResource_.Get(); }
    ID3D12Resource* GetPostEffectResultResource() const { return postEffectResultResource_.Get(); }

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
    void InitializeFullscreenPostEffectParameter();
    void InitializeFence();
    void InitializeDXCCompiler();
    void InitializeFixFPS();
    void UpdateFixFPS();
    void TransitionRenderTexture(D3D12_RESOURCE_STATES stateAfter);
    void TransitionPostEffectResult(D3D12_RESOURCE_STATES stateAfter);
    void DrawFullscreenTriangle(
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
        FullscreenPostEffectType postEffectType);

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
    Microsoft::WRL::ComPtr<ID3D12Resource> postEffectResultResource_;
    D3D12_RESOURCE_STATES postEffectResultState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    // RTVヒープ内での場所（0,1はモニター用、2はゲーム画面用）
    const UINT kRenderTextureRTVIndex = 2;
    const UINT kPostEffectResultRTVIndex = 3;
    uint32_t nextRtvIndex_ = 4; // RTVの次の割り当てインデックス

    static constexpr DXGI_FORMAT kRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr size_t kFullscreenPostEffectCount = static_cast<size_t>(FullscreenPostEffectType::Count);
    float renderTextureClearColor_[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

    Microsoft::WRL::ComPtr<ID3D12RootSignature> copyImageRootSignature_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, kFullscreenPostEffectCount> fullscreenPostEffectPipelineStates_;
    Microsoft::WRL::ComPtr<ID3D12Resource> fullscreenPostEffectParameterResource_;
    FullscreenPostEffectParameter* mappedFullscreenPostEffectParameter_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = {};
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
    std::chrono::steady_clock::time_point reference_;
};
