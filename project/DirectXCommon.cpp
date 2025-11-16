#include "DirectXCommon.h"

#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp) {

    // NULL検出
    assert(winApp);

    // メンバ変数に記録
    this->winApp_ = winApp;

    InitializeDevice();
    InitializeCommand();
    InitializeSwapChain(winApp);
    InitializeDescriptorHeaps();
    InitializeRenderTargetView();
    InitializeDepthBuffer();
    InitializeDepthStencilView();
    InitializeViewport();
    InitializeScissorRect();
    InitializeDXCCompiler();
    InitializeImGui();
    InitializeFence();
	
}

void DirectXCommon::PreDraw() {
    // バックバッファの番号取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // リソースバリアで描画可能状態に変更
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    // RTV / DSV のハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(descriptorSizeRTV_) * backBufferIndex;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    // 描画先の設定
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // 画面クリア
    float clearColor[4] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(
        dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // ビューポート / シザー
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw() {
    // バックバッファの番号取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // 描画終了 → 表示用にリソースバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    // コマンドリストを閉じてGPUに投げる
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, cmdLists);

    // 画面に表示
    hr = swapChain_->Present(1, 0);
    assert(SUCCEEDED(hr));

    // Fence 同期
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));

    if (fence_->GetCompletedValue() < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        assert(SUCCEEDED(hr));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // 次フレーム用にリセット
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) const {
    return GetCPUDescriptorHandle(srvDescriptorHeap_, srvDescriptorSize_, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) const {
    return GetGPUDescriptorHandle(srvDescriptorHeap_, srvDescriptorSize_, index);
}

void DirectXCommon::InitializeDevice() {
    HRESULT hr;

#if _DEBUG
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    // DXGIファクトリーの作成
    hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // ハードウェアアダプタ取得（メンバの adapter_ に格納）
    hr = dxgiFactory_->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&adapter_)
    );
    assert(SUCCEEDED(hr));

    // デバイス作成
    hr = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
    assert(SUCCEEDED(hr));
}


void DirectXCommon::InitializeCommand() {
    HRESULT hr;

    // コマンドキュー
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    // コマンドアロケータ
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));

    // コマンドリスト
    hr = device_->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(), nullptr,
        IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
}


void DirectXCommon::InitializeSwapChain(WinApp* winApp) {
    // ← スライド「スワップチェーンの生成」のコードをここに
}

void DirectXCommon::InitializeRenderTargetView() {
    // ← スライド「レンダーターゲットビューの初期化」
    // backBuffers_ にリソースを入れて、rtvHeap_ にビューを作る
}

void DirectXCommon::InitializeDepthBuffer() {
    // ← スライド「深度バッファの生成」「深度ステンシルビューの初期化」
}

void DirectXCommon::InitializeDescriptorHeaps() {
    // TODO: 資料の「RTV/SRV/DSV デスクリプタヒープの作成」をここに書く
}

void DirectXCommon::InitializeDepthStencilView() {
    // TODO: 資料の「深度ステンシルビューの作成」をここに書く
}

void DirectXCommon::InitializeViewport() {
    // TODO: 資料の「ビューポートの設定」をここに書く
}

void DirectXCommon::InitializeScissorRect() {
    // TODO: 資料の「シザー矩形の設定」をここに書く
}

void DirectXCommon::InitializeDXCCompiler() {
    // TODO: 資料の「DXC コンパイラの初期化」をここに書く
}

void DirectXCommon::InitializeImGui() {
    // TODO: 資料の「ImGui の初期化」をここに書く
}


void DirectXCommon::InitializeFence() {
    // ← フェンス生成の処理
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, UINT descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, UINT descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize) * index;
    return handle;
}

