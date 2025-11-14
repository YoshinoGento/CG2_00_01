#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Logger.h"
#include "WinApp.h"
#include "StringUtility.h"

class WinApp;    // 前方宣言（ヘッダで WinApp.h をincludeしてもOK）

// DirectX の基盤クラス
class DirectXCommon {
public:
    // 初期化（必ず WinApp のポインタを渡す）
    void Initialize(WinApp* winApp);

    // ここではまだ中身を書かないけど、将来用
    void Finalize() {}   // 必要になったら解放処理を書き足す

    // デバイスやコマンドリストを他のクラスから触りたい時用の getter
    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }
    IDXGISwapChain4* GetSwapChain() const { return swapChain.Get(); }

private:
    // === Initialize の中から呼ばれる分割関数 ===
    void InitDevice();          // デバイスまわりの初期化
    void InitCommand();         // コマンドキュー / アロケータ / リスト
    void InitSwapChain();       // スワップチェーン
    void InitDepthBuffer();     // 深度バッファ
    void InitDescriptorHeaps(); // RTV / DSV / SRV のディスクリプタヒープ
    void InitScissorRect();
    void InitDXC();
    void InitImGui();

    // ディスクリプタヒープをまとめて作る補助関数
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);

private:
    // === 依存関係 ===
    WinApp* winApp_ = nullptr;  // HWND をもらうために WinApp を保持

    // === よく使う DirectX オブジェクトをメンバとして保持 ===
    // Device / Factory
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

    // Command 関連
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

    // SwapChain
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2];

    // DescriptorHeap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;

    // DepthBuffer
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

    // DXC（HLSLのコンパイルに必要なオブジェクト）
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;              // ユーティリティ（ファイル読み込みなど）
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;       // コンパイラ本体
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_; // #include 対応


    // ディスクリプタサイズ（後で使うので持っておく）
    UINT descriptorSizeRTV = 0;
    UINT descriptorSizeDSV = 0;
    UINT descriptorSizeSRV = 0;


    D3D12_RECT scissorRect_;
};
