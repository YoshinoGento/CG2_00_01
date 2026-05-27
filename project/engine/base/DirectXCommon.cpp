#include "DirectXCommon.h"
#include "Logger.h"
#include <cassert>
#include <vector>
#include <thread>
#include "externals/DirectXTex/d3dx12.h" 

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

// --- 文字列変換ヘルパー ---
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

// --- 初期化メイン ---
void DirectXCommon::Initialize(WinApp* winApp) {
    assert(winApp);
    winApp_ = winApp;

    InitializeFixFPS();           // FPS固定準備
    InitializeDevice();           // GPUの準備
    InitializeCommand();          // 命令リストの準備
    InitializeSwapChain(winApp);  // 画面入れ替え設定
    InitializeRenderTargetView(); // 描き込み先管理(RTV)の準備
    InitializeDepthStencilView(); // 奥行き(Zバッファ)の準備
    InitializeRenderTexture();    // ★エディタ用：ゲーム画面用テクスチャの作成
    InitializeFence();            // CPU/GPU同期の準備
    InitializeDXCCompiler();      // シェーダー翻訳機の準備
}

// --- 初期化の詳細実装（LNK2019対策：すべて記述） ---

void DirectXCommon::InitializeDevice() {
#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif
    CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        if (SUCCEEDED(D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr))) break;
    }
    D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device_));
}

void DirectXCommon::InitializeCommand() {
    device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
}

void DirectXCommon::InitializeSwapChain(WinApp* winApp) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kClientWidth; swapChainDesc.Height = WinApp::kClientHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
}

void DirectXCommon::InitializeRenderTargetView() {
    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // スワップチェーン用2個 + ビューポート用1個 + 追加エフェクト等用に多めに確保(64個)
    rtvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, false);
    for (uint32_t i = 0; i < 2; ++i) {
        swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<size_t>(i) * rtvDescriptorSize_;
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
    }
}

void DirectXCommon::InitializeDepthStencilView() {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = WinApp::kClientWidth; desc.Height = WinApp::kClientHeight;
    desc.MipLevels = 1; desc.DepthOrArraySize = 1; desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_HEAP_PROPERTIES prop{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE clear{}; clear.DepthStencil.Depth = 1.0f; clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    device_->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&depthBuffer_));
    dsvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    device_->CreateDepthStencilView(depthBuffer_.Get(), nullptr, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::InitializeRenderTexture() {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = WinApp::kClientWidth; desc.Height = WinApp::kClientHeight;
    desc.MipLevels = 1; desc.DepthOrArraySize = 1; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE clearValue{}; clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.1f; clearValue.Color[1] = 0.25f; clearValue.Color[2] = 0.5f; clearValue.Color[3] = 1.0f;
    device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue, IID_PPV_ARGS(&renderTextureResource_));
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(2) * rtvDescriptorSize_; // Index 2 を使用
    device_->CreateRenderTargetView(renderTextureResource_.Get(), nullptr, rtvHandle);
}

void DirectXCommon::InitializeFence() {
    device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void DirectXCommon::InitializeDXCCompiler() {
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
}

// --- 描画フロー管理 ---

/**
 * PreDraw: ゲーム画面をテクスチャへ描き込む準備
 */
void DirectXCommon::PreDraw() {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTextureResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(2) * rtvDescriptorSize_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport{ 0, 0, (FLOAT)WinApp::kClientWidth, (FLOAT)WinApp::kClientHeight, 0, 1 };
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissor{ 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
    commandList_->RSSetScissorRects(1, &scissor);
}

/**
 * PreDrawToSwapChain: 描き込み先を実際のモニターへ切り替える
 */
void DirectXCommon::PreDrawToSwapChain() {
    D3D12_RESOURCE_BARRIER barrierRT{};
    barrierRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierRT.Transition.pResource = renderTextureResource_.Get();
    barrierRT.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierRT.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList_->ResourceBarrier(1, &barrierRT);

    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER barrierSC{};
    barrierSC.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierSC.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrierSC.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrierSC.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrierSC);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(backBufferIndex) * rtvDescriptorSize_;
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

/**
 * PostDraw: GPUへの命令送信と同期
 */
void DirectXCommon::PostDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffers_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    commandList_->Close();
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
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);
}

// --- リソース管理ヘルパー ---

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const std::wstring& profile) {
    std::wstring directory = filePath.substr(0, filePath.find_last_of(L"\\/"));
    std::wstring includePath = L"-I" + directory;
    IDxcBlobEncoding* shaderSource = nullptr;
    dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    DxcBuffer shaderSourceBuffer{ shaderSource->GetBufferPointer(), shaderSource->GetBufferSize(), DXC_CP_UTF8 };
    LPCWSTR arguments[] = { filePath.c_str(), L"-E", L"main", L"-T", profile.c_str(), L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr", includePath.c_str() };
    IDxcResult* shaderResult = nullptr;
    dxcCompiler_->Compile(&shaderSourceBuffer, arguments, _countof(arguments), includeHandler_.Get(), IID_PPV_ARGS(&shaderResult));
    IDxcBlobEncoding* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobEncoding), (void**)&shaderError, nullptr);
    if (shaderError != nullptr && shaderError->GetBufferSize() != 0) {
        Logger::Log((char*)shaderError->GetBufferPointer());
        assert(false);
    }
    IDxcBlob* shaderBlob = nullptr;
    shaderResult->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void**)&shaderBlob, nullptr);
    shaderSource->Release(); shaderResult->Release(); shaderError->Release();
    return ComPtr<IDxcBlob>(shaderBlob);
}

DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath) {
    DirectX::ScratchImage image; std::wstring wFilePath = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    if (FAILED(hr)) { hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1); image.GetPixels()[0] = 255; }
    return image;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = UINT(metadata.width); desc.Height = UINT(metadata.height); desc.MipLevels = UINT16(metadata.mipLevels);
    desc.DepthOrArraySize = UINT16(metadata.arraySize); desc.Format = metadata.format; desc.SampleDesc.Count = 1;
    desc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_DEFAULT };
    ComPtr<ID3D12Resource> resource;
    device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
    return resource;
}

void DirectXCommon::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
    uint64_t size = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
    ComPtr<ID3D12Resource> intermediate = CreateBufferResource(size);
    UpdateSubresources(commandList_.Get(), texture, intermediate.Get(), 0, 0, UINT(subresources.size()), subresources.data());
    D3D12_RESOURCE_BARRIER barrier{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE };
    barrier.Transition.pResource = texture; 
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; 
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    PostDraw(); // 同期のために一旦実行
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = sizeInBytes; desc.Height = 1; desc.DepthOrArraySize = 1;
    desc.MipLevels = 1; desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> resource;
    device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    return resource;
}

ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{ type, numDescriptors, shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
    ComPtr<ID3D12DescriptorHeap> heap; device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    return heap;
}

uint32_t DirectXCommon::AllocateRTV() {
    uint32_t index = nextRtvIndex_;
    nextRtvIndex_++;
    return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<size_t>(index) * rtvDescriptorSize_;
    return handle;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const float* clearColor) {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.DepthOrArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    if (clearColor) {
        clearValue.Color[0] = clearColor[0];
        clearValue.Color[1] = clearColor[1];
        clearValue.Color[2] = clearColor[2];
        clearValue.Color[3] = clearColor[3];
    } else {
        clearValue.Color[0] = 0.0f; clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f; clearValue.Color[3] = 1.0f;
    }

    ComPtr<ID3D12Resource> resource;
    device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue, IID_PPV_ARGS(&resource));
    return resource;
}

// --- FPS制御 ---
void DirectXCommon::InitializeFixFPS() { reference_ = std::chrono::steady_clock::now(); }
void DirectXCommon::UpdateFixFPS() {
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - reference_ < kMinTime) std::this_thread::sleep_until(reference_ + kMinTime);
    reference_ = std::chrono::steady_clock::now();
}