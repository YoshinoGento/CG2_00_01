#include "DirectXCommon.h"
#include "WinApp.h"

// 標準
#include <cassert>
#include <format>
#include <vector>

// DirectX12
#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// DXC（HLSLコンパイル）
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

// ImGui
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"
using Microsoft::WRL::ComPtr;

//===============================
// Initialize
//===============================
void DirectXCommon::Initialize(WinApp* winApp) {
    // WinApp が渡ってきているかチェック
    assert(winApp);

    // メンバ変数に記録しておく（あとで必要）
    this->winApp_ = winApp;

    // ======== ここから DirectX の初期化手順 =========

    InitDevice();                   // デバイス生成（GPUを使う準備）
    InitCommand();                  // コマンドキュー／アロケータ／リスト生成
    InitSwapChain();                // スワップチェーン（画面の裏表バッファ）
    InitDescriptorHeaps();          // RTV / SRV / DSV のヒープ作成
    //InitRenderTargetView();         // 画面描画用 RTV の作成
    InitDepthBuffer();              // 深度バッファ作成
    //InitDepthStencilView();         // DSV（深度ステンシルビュー）作成
    //InitFence();                    // GPU同期用フェンス
    //InitViewport();                 // ビューポート（描画範囲）
    InitScissorRect();              // シザリング矩形 ← 今追加した部分
    InitDXC();                      // DXCコンパイラ ← 今追加した部分
    InitImGui();                    // ImGui ← 今追加した部分

    // ======== ここまで初期化処理 =========
}


//===============================
// ① デバイスの初期化
//===============================
void DirectXCommon::InitDevice() {
    HRESULT hr;

#ifdef _DEBUG
    // デバッグレイヤーを ON にする
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    // DXGIファクトリーの生成
    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
    assert(SUCCEEDED(hr));

    // 使用するアダプタを探す（ハイパフォーマンス優先）
    ComPtr<IDXGIAdapter4> useAdapter;
    for (UINT i = 0;
        dxgiFactory->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
        ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));

        // ソフトウェアアダプタはスキップ
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            useAdapter.Reset();
            continue;
        }

        Logger::Log(
            "Use Adapter : " +
            StringUtility::ConvertString(adapterDesc.Description)); // wstring -> string
        break;
    }
    assert(useAdapter); // 適切なアダプタが見つからなかったら落とす

    // 機能レベル候補
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
    };
    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

    // 高いレベルから順にデバイス生成を試す
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(
            useAdapter.Get(), featureLevels[i],
            IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) {
            Logger::Log(std::string("FeatureLevel : ") + featureLevelStrings[i]);
            break;
        }
    }
    assert(device); // デバイス生成失敗は致命的

    Logger::Log("Complete create D3D12Device!!!");

#ifdef _DEBUG
    // InfoQueue 設定（やばいエラーで止める）
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        infoQueue->PushStorageFilter(&filter);
    }
#endif
}

//===============================
// ② コマンド関連の初期化
//===============================
void DirectXCommon::InitCommand() {
    HRESULT hr;

    // コマンドキュー作成
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 直接コマンドリストを投げられるキュー
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    assert(SUCCEEDED(hr));

    // コマンドアロケータ作成
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator));
    assert(SUCCEEDED(hr));

    // コマンドリスト作成
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList));
    assert(SUCCEEDED(hr));

    Logger::Log("Command関連の初期化完了");
}

//===============================
// ③ スワップチェーンの生成
//===============================
void DirectXCommon::InitSwapChain() {
    HRESULT hr;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kClientWidth;
    swapChainDesc.Height = WinApp::kClientHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // HWND は WinApp 経由で取得
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        winApp_->GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
    assert(SUCCEEDED(hr));

    Logger::Log("SwapChain の生成完了");

    // バックバッファの Resource を取っておく
    for (UINT i = 0; i < 2; ++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
        assert(SUCCEEDED(hr));
    }
}

//===============================
// ④ 深度バッファの生成
//===============================
void DirectXCommon::InitDepthBuffer() {
    HRESULT hr;

    // Resource の設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // ヒープ設定
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // クリア値
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilResource));
    assert(SUCCEEDED(hr));

    Logger::Log("DepthBuffer の生成完了");
}

//===============================
// ⑤ ディスクリプタヒープの生成
//===============================
void DirectXCommon::InitDescriptorHeaps() {
    // 各 DescriptorSize を取得しておく
    descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // RTV 用ヒープ（バッファ2枚ぶん）
    rtvDescriptorHeap = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        2,
        false);

    // DSV 用ヒープ（1個）
    dsvDescriptorHeap = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        1,
        false);

    // SRV 用ヒープ（とりあえず 128 個分確保（君のコードに合わせた））
    srvDescriptorHeap = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        128,
        true);

    Logger::Log("DescriptorHeap の生成完了");
}

void DirectXCommon::InitScissorRect() {
    // シザリング矩形（描画する範囲）を設定する
    // → 後でコマンドリストにセットして使用する
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;   // 画面幅いっぱい
    scissorRect_.bottom = WinApp::kClientHeight;  // 画面高さいっぱい
}

void DirectXCommon::InitDXC() {
    HRESULT hr;

    // DXC Utility の生成（ファイルの読み込みなどができるオブジェクト）
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));  // 失敗したら強制停止

    // DXC コンパイラの生成（HLSL → DXIL にコンパイルする本体）
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // デフォルトのインクルードハンドラを作成
    // → #include で HLSL を読み込む時に必要
    hr = dxcUtils_->CreateDefaultIncludeHandler(&dxcIncludeHandler_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitImGui() {
    // ImGui のバージョンチェック
    IMGUI_CHECKVERSION();

    // ImGui コンテキスト生成（内部状態を作る）
    ImGui::CreateContext();

    // ダークテーマのスタイル適用
    ImGui::StyleColorsDark();

    // Win32（ウィンドウ）用の初期化
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // DirectX12 用の初期化
    // ※ SRVディスクリプタヒープを渡す必要がある
    ImGui_ImplDX12_Init(
        device.Get(),
        2, // バッファ数
        DXGI_FORMAT_R8G8B8A8_UNORM, // スワップチェーンのフォーマット
        srvDescriptorHeap.Get(),    // SRVヒープのアドレス
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
    );
}



//===============================
// ディスクリプタヒープ作成関数
//===============================
ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ComPtr<ID3D12DescriptorHeap> heap;
    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    assert(SUCCEEDED(hr));
    return heap;
}
