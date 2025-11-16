#pragma once

// ほかのクラスから使うので
#include "WinApp.h"

#include <array>        // ← バックバッファをstd::arrayで持つなら必要
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>

// DirectX基盤
class DirectXCommon {
public:
    //=========================
    //  公開メンバ関数
    //=========================

    /// <summary>
    /// DirectXの初期化
    /// </summary>
    void Initialize(WinApp* winApp);

    // 描画前処理
    void PreDraw();

    // 描画後処理
    void PostDraw();

    // SRV用CPUデスクリプタハンドルを取得する
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;

    // SRV用GPUデスクリプタハンドルを取得する
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

private:
    //=========================
    //  内部用初期化関数
    //=========================
    void InitializeDevice();                 // デバイスの生成
    void InitializeCommand();                // コマンド関連の生成
    void InitializeSwapChain(WinApp* winApp);// スワップチェーンの生成
    void InitializeRenderTargetView();       // RTVの生成
    void InitializeDepthBuffer();            // 深度バッファ生成
    void InitializeFence();                  // フェンス生成
    void InitializeDescriptorHeaps();
    void InitializeDepthStencilView();
    void InitializeViewport();
    void InitializeScissorRect();
    void InitializeDXCCompiler();
    void InitializeImGui();

    // （深度ステンシルビューの初期化を分けるなら）
    // void InitializeDepthStencilView();

private:
    //=========================
    //  メンバ変数
    //=========================

    // WinAPI（スワップチェーン作るのに必要）
    WinApp* winApp_ = nullptr;

    // --- デバイスまわり ---
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    // --- DXGIファクトリ & アダプタ ---
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;

    // --- コマンドまわり ---
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>         commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>  commandList_;

    // --- スワップチェーン & バックバッファ ---
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> backBuffers_;

    // --- RTV（Render Target View）---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    UINT rtvDescriptorSize_ = 0;

    // --- DSV（DepthStencilView / 深度バッファ）---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource>       depthBuffer_;

    // --- フェンス ---
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // SRV用デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    UINT srvDescriptorSize_ = 0;

    // --- デスクリプタヒープ ---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

    UINT descriptorSizeRTV_ = 0;
    UINT descriptorSizeSRV_ = 0;
    UINT descriptorSizeDSV_ = 0;

    // --- ビューポート / シザー ---
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    // --- DXC シェーダーコンパイラ ---
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;


    /// <summary>
   /// 任意のCPUデスクリプタハンドルを取得する
   /// </summary>
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        UINT descriptorSize,
        uint32_t index);

    /// <summary>
    /// 任意のGPUデスクリプタハンドルを取得する
    /// </summary>
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        UINT descriptorSize,
        uint32_t index);
};
