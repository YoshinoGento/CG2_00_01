#include "DirectXCommon.h"
#include "Logger.h"

#include <cassert>
#include <vector>
#include <thread>
// #include <format> // C++20非対応環境対策のため削除
#include "externals/DirectXTex/d3dx12.h" 

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

// 文字列変換ヘルパー
std::wstring ConvertString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (sizeNeeded == 0) return std::wstring();
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) return std::string();
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], sizeNeeded, NULL, NULL);
    return result;
}

void DirectXCommon::Initialize(WinApp* winApp) {
    assert(winApp);
    winApp_ = winApp;

    InitializeFixFPS();
    InitializeDevice();
    InitializeCommand();
    InitializeSwapChain(winApp);
    InitializeRenderTargetView();
    InitializeDepthStencilView();
    InitializeFence();
    InitializeDXCCompiler();

    // SRVヒープ初期化
    // srvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
    // srvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DirectXCommon::PreDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(backBufferIndex) * rtvDescriptorSize_;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport{};
    viewport.Width = (FLOAT)WinApp::kClientWidth;
    viewport.Height = (FLOAT)WinApp::kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.right = WinApp::kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = WinApp::kClientHeight;
    commandList_->RSSetScissorRects(1, &scissorRect);
}

void DirectXCommon::PostDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);

    swapChain_->Present(1, 0);

    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    UpdateFixFPS();

    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (static_cast<UINT64>(index) * srvDescriptorSize_);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (static_cast<UINT64>(index) * srvDescriptorSize_);
    return handle;
}

uint32_t DirectXCommon::AllocateSRVIndex() {
    return 0;
}

DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath) {
    DirectX::ScratchImage image;
    std::wstring wFilePath = ConvertString(filePath);

    // 1. 指定されたファイルを読み込んでみる
    HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

    // 2. もし読み込みに失敗したら...
    if (FAILED(hr)) {
        Logger::Log("Texture load failed: " + filePath);
        Logger::Log("Generating DUMMY White Texture...");

        // 真っ白な1x1ピクセルの画像をその場で作って代用する（これでエラー回避！）
        hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        assert(SUCCEEDED(hr));

        // 白で塗りつぶす (R=255, G=255, B=255, A=255)
        uint8_t* pData = image.GetPixels();
        pData[0] = 255; // R
        pData[1] = 255; // G
        pData[2] = 255; // B
        pData[3] = 255; // A
    }

    return image;
}

// ★復元: CreateTextureResource
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT16(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // データ転送用初期状態
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

// ★復元: UploadTextureData
void DirectXCommon::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

    uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);

    UpdateSubresources(commandList_.Get(), texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

    // リソースステートを COPY_DEST から GENERIC_READ へ変更
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // ★コマンドを実行してGPU側への転送完了を待つ (簡易実装)
    // 中間リソースが破棄されないようにするため、ここでWaitする
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);

    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // コマンドリストをリセットして再開可能にする
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const std::wstring& profile) {
    // ログ出力 (std::formatを使わない形に修正)
    Logger::Log("Compiling Shader: " + ConvertString(filePath));

    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);

    if (FAILED(hr)) {
        Logger::Log("FATAL ERROR: Failed to load shader file.");
        Logger::Log("File Path: " + ConvertString(filePath));
        assert(false);
        return nullptr;
    }

    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile.c_str(),
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr",
    };

    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler_.Get(),
        IID_PPV_ARGS(&shaderResult)
    );
    assert(SUCCEEDED(hr));

    // エラーメッセージ取得
    IDxcBlobEncoding* shaderError = nullptr;
    // ★修正: IID_PPV_ARGS を使わず、明示的にキャストして安全に取得
    shaderResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobEncoding), (void**)&shaderError, nullptr);

    if (shaderError != nullptr && shaderError->GetBufferSize() != 0) {
        Logger::Log("Shader Compile Error:");
        Logger::Log((char*)shaderError->GetBufferPointer());
        assert(false);
    }

    // コンパイル結果取得
    IDxcBlob* shaderBlob = nullptr;
    // ★修正: IID_PPV_ARGS を使わず、明示的にキャスト
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void**)&shaderBlob, nullptr);
    assert(SUCCEEDED(hr));

    // 解放
    if (shaderSource) shaderSource->Release();
    if (shaderResult) shaderResult->Release();
    if (shaderError) shaderError->Release();

    return Microsoft::WRL::ComPtr<IDxcBlob>(shaderBlob);
}

// --- 初期化関数群 ---

void DirectXCommon::InitializeFixFPS() {
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    if (elapsed < kMinTime) {
        std::chrono::steady_clock::time_point waitEnd = reference_ + kMinTime;
        std::this_thread::sleep_until(waitEnd);
    }
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::InitializeDevice() {
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        if (FAILED(hr)) continue;
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr))) break;
    }
    if (useAdapter == nullptr) {
        for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
            if (SUCCEEDED(D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr))) break;
        }
    }
    assert(useAdapter != nullptr);
    hr = D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeCommand() {
    HRESULT hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));

    // ★完全修正箇所: 変数を定義して、そのアドレスを渡す (C2102回避)
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    // &commandQueue_ を使用して安全に渡す
    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeSwapChain(WinApp* winApp) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kClientWidth;
    swapChainDesc.Height = WinApp::kClientHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeRenderTargetView() {
    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rtvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    for (uint32_t i = 0; i < 2; ++i) {
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        assert(SUCCEEDED(hr));
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<size_t>(i) * rtvDescriptorSize_;
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
    }
}

void DirectXCommon::InitializeDepthStencilView() {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&depthBuffer_));
    assert(SUCCEEDED(hr));

    dsvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    device_->CreateDepthStencilView(depthBuffer_.Get(), nullptr, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::InitializeFence() {
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeDXCCompiler() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bufferResourceDesc{};
    bufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferResourceDesc.Width = sizeInBytes;
    bufferResourceDesc.Height = 1;
    bufferResourceDesc.DepthOrArraySize = 1;
    bufferResourceDesc.MipLevels = 1;
    bufferResourceDesc.SampleDesc.Count = 1;
    bufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = type;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
}