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

/// <summary>
/// DirectX12の基本機能を管理するクラス
/// デバイスの生成、コマンドリストの管理、スワップチェーン、描画の前後処理などを担当
/// </summary>
class DirectXCommon {
public:
    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="winApp">ウィンドウ管理クラスへのポインタ</param>
    void Initialize(WinApp* winApp);

    /// <summary>
    /// 描画前処理
    /// バックバッファの切り替えや画面クリアを行う
    /// </summary>
    void PreDraw();

    /// <summary>
    /// 描画後処理
    /// コマンドの実行完了を待ち、画面を表示（Present）する
    /// </summary>
    void PostDraw();

    // --- ディスクリプタハンドル取得 ---

    /// <summary>
    /// SRVのCPUディスクリプタハンドルを取得
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;

    /// <summary>
    /// SRVのGPUディスクリプタハンドルを取得
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

    // --- 各種ゲッター群 ---

    /// <summary>
    /// SRV用ディスクリプタヒープの取得
    /// </summary>
    ID3D12DescriptorHeap* GetSrvHeap() const { return srvDescriptorHeap_.Get(); }

    /// <summary>
    /// デバイスの取得
    /// </summary>
    ID3D12Device* GetDevice() const { return device_.Get(); }

    /// <summary>
    /// コマンドリストの取得
    /// </summary>
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    /// <summary>
    /// コマンドキューの取得（ImGuiManagerなどで使用）
    /// </summary>
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

    /// <summary>
    /// スワップチェーンのバックバッファ数を取得（ImGuiManagerなどで使用）
    /// </summary>
    size_t GetSwapChainResourcesNum() const { return backBuffers_.size(); }

    // --- リソース管理・ヘルパー ---

    /// <summary>
    /// SRVヒープの空きインデックスを確保（現在は簡易実装）
    /// </summary>
    uint32_t AllocateSRVIndex();

    /// <summary>
    /// テクスチャファイルを読み込む
    /// </summary>
    DirectX::ScratchImage LoadTexture(const std::string& filePath);

    /// <summary>
    /// テクスチャリソースの生成
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

    /// <summary>
    /// テクスチャデータをGPUにアップロード
    /// </summary>
    void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

    /// <summary>
    /// シェーダーのコンパイル
    /// </summary>
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& profile);

    /// <summary>
    /// バッファリソース（定数バッファ等）の生成
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    /// <summary>
    /// 各種ディスクリプタヒープの生成
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);

private:
    // --- 内部初期化メソッド ---

    void InitializeFixFPS();      // FPS固定初期化
    void UpdateFixFPS();          // FPS固定更新
    void InitializeDevice();      // デバイス生成
    void InitializeCommand();     // コマンド関連初期化
    void InitializeSwapChain(WinApp* winApp); // スワップチェーン生成
    void InitializeRenderTargetView();   // RTV初期化
    void InitializeDepthStencilView();    // DSV初期化
    void InitializeFence();       // フェンス初期化
    void InitializeDXCCompiler(); // DXCシェーダーコンパイラ初期化

    // メンバ変数
    WinApp* winApp_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> backBuffers_; // ダブルバッファリング用

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    UINT rtvDescriptorSize_ = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
    UINT dsvDescriptorSize_ = 0;

    // SRV用ヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    UINT srvDescriptorSize_ = 0;

    // 同期用
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = {};

    // シェーダーコンパイル用
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

    // FPS制御
    std::chrono::steady_clock::time_point reference_;
};