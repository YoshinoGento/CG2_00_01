#include "DirectXCommon.h"
#include "Logger.h"
#include <algorithm>
#include <cassert>
#include <vector>
#include <thread>
#include "externals/DirectXTex/d3dx12.h" 
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3dcompiler.lib")

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
    desc.MipLevels = 1; desc.DepthOrArraySize = 1; desc.Format = kRenderTargetFormat;
    desc.SampleDesc.Count = 1; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_CLEAR_VALUE clearValue{}; clearValue.Format = kRenderTargetFormat;
    clearValue.Color[0] = renderTextureClearColor_[0];
    clearValue.Color[1] = renderTextureClearColor_[1];
    clearValue.Color[2] = renderTextureClearColor_[2];
    clearValue.Color[3] = renderTextureClearColor_[3];
    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&renderTextureResource_));
    assert(SUCCEEDED(hr));
    renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(kRenderTextureRTVIndex) * rtvDescriptorSize_;
    device_->CreateRenderTargetView(renderTextureResource_.Get(), nullptr, rtvHandle);

    float postEffectClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValue.Color[0] = postEffectClearColor[0];
    clearValue.Color[1] = postEffectClearColor[1];
    clearValue.Color[2] = postEffectClearColor[2];
    clearValue.Color[3] = postEffectClearColor[3];
    hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&postEffectResultResource_));
    assert(SUCCEEDED(hr));
    postEffectResultState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_CPU_DESCRIPTOR_HANDLE postEffectRtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    postEffectRtvHandle.ptr += static_cast<size_t>(kPostEffectResultRTVIndex) * rtvDescriptorSize_;
    device_->CreateRenderTargetView(postEffectResultResource_.Get(), nullptr, postEffectRtvHandle);
}

void DirectXCommon::InitializeCopyImagePipeline() {
    auto vsBlob = CompileShader(L"Resources/shader/CopyImage.VS.hlsl", L"vs_6_0");

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.MipLODBias = 0.0f;
    staticSampler.MaxAnisotropy = 1;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    staticSampler.MinLOD = 0.0f;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.RegisterSpace = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &staticSampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(std::string(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize()));
        }
        assert(false);
    }

    hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&copyImageRootSignature_));
    assert(SUCCEEDED(hr));

    auto createPipelineState = [&](const std::wstring& psPath) {
        auto psBlob = CompileShader(psPath, L"ps_6_0");

        D3D12_BLEND_DESC blendDesc{};
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        rasterizerDesc.DepthClipEnable = TRUE;

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = FALSE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = copyImageRootSignature_.Get();
        pipelineDesc.InputLayout = { nullptr, 0 };
        pipelineDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        pipelineDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        pipelineDesc.BlendState = blendDesc;
        pipelineDesc.RasterizerState = rasterizerDesc;
        pipelineDesc.DepthStencilState = depthStencilDesc;
        pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineDesc.NumRenderTargets = 1;
        pipelineDesc.RTVFormats[0] = kRenderTargetFormat;
        pipelineDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pipelineDesc.SampleDesc.Count = 1;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
        HRESULT psoHr = device_->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
        assert(SUCCEEDED(psoHr));
        return pipelineState;
        };

    fullscreenPostEffectPipelineStates_[static_cast<size_t>(FullscreenPostEffectType::Copy)] =
        createPipelineState(L"Resources/shader/CopyImage.PS.hlsl");
    fullscreenPostEffectPipelineStates_[static_cast<size_t>(FullscreenPostEffectType::Grayscale)] =
        createPipelineState(L"Resources/shader/Grayscale.PS.hlsl");
    fullscreenPostEffectPipelineStates_[static_cast<size_t>(FullscreenPostEffectType::Sepia)] =
        createPipelineState(L"Resources/shader/Sepia.PS.hlsl");
    fullscreenPostEffectPipelineStates_[static_cast<size_t>(FullscreenPostEffectType::Blur)] =
        createPipelineState(L"Resources/shader/Blur.PS.hlsl");
    fullscreenPostEffectPipelineStates_[static_cast<size_t>(FullscreenPostEffectType::Bloom)] =
        createPipelineState(L"Resources/shader/Bloom.PS.hlsl");
    fullscreenPostEffectPipelineStates_[static_cast<size_t>(FullscreenPostEffectType::Vignette)] =
        createPipelineState(L"Resources/shader/Vignette.PS.hlsl");
}

void DirectXCommon::InitializeFence() {
    device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void DirectXCommon::InitializeDXCCompiler() {
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    InitializeCopyImagePipeline();
    InitializeFullscreenPostEffectParameter();
}

void DirectXCommon::InitializeFullscreenPostEffectParameter() {
    constexpr size_t kConstantBufferSize = 256;
    fullscreenPostEffectParameterResource_ = CreateBufferResource(kConstantBufferSize);
    vignetteParameterResource_ = CreateBufferResource(kConstantBufferSize);

    D3D12_RANGE readRange{ 0, 0 };
    HRESULT hr = fullscreenPostEffectParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedFullscreenPostEffectParameter_));
    assert(SUCCEEDED(hr));
    hr = vignetteParameterResource_->Map(
        0, &readRange, reinterpret_cast<void**>(&mappedVignetteParameter_));
    assert(SUCCEEDED(hr));

    FullscreenPostEffectParameter parameter{};
    SetFullscreenPostEffectParameter(parameter);
    VignetteParamForGPU vignetteParameter{};
    SetVignetteParameter(vignetteParameter);
}

void DirectXCommon::SetFullscreenPostEffectParameter(const FullscreenPostEffectParameter& parameter) {
    if (!mappedFullscreenPostEffectParameter_) {
        return;
    }

    mappedFullscreenPostEffectParameter_->grayscaleIntensity = std::clamp(parameter.grayscaleIntensity, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->sepiaIntensity = std::clamp(parameter.sepiaIntensity, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->blurStrength = std::clamp(parameter.blurStrength, 0.0f, 16.0f);
    mappedFullscreenPostEffectParameter_->padding = 0.0f;
    mappedFullscreenPostEffectParameter_->bloomThreshold = std::clamp(parameter.bloomThreshold, 0.0f, 1.0f);
    mappedFullscreenPostEffectParameter_->bloomIntensity = std::clamp(parameter.bloomIntensity, 0.0f, 8.0f);
    mappedFullscreenPostEffectParameter_->bloomRadius = std::clamp(parameter.bloomRadius, 0.0f, 32.0f);
    mappedFullscreenPostEffectParameter_->bloomSoftKnee = std::clamp(parameter.bloomSoftKnee, 0.001f, 1.0f);
}

void DirectXCommon::SetVignetteParameter(const VignetteParamForGPU& parameter) {
    if (!mappedVignetteParameter_) {
        return;
    }

    mappedVignetteParameter_->scale = std::clamp(parameter.scale, 0.0f, 64.0f);
    mappedVignetteParameter_->power = std::clamp(parameter.power, 0.01f, 8.0f);
    mappedVignetteParameter_->intensity = std::clamp(parameter.intensity, 0.0f, 1.0f);
    mappedVignetteParameter_->padding = 0.0f;
}

// --- 描画フロー管理 ---

/**
 * PreDraw: ゲーム画面をテクスチャへ描き込む準備
 */
void DirectXCommon::PreDraw() {
    assert(renderTextureResource_);
    TransitionRenderTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<size_t>(kRenderTextureRTVIndex) * rtvDescriptorSize_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    commandList_->ClearRenderTargetView(rtvHandle, renderTextureClearColor_, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport{ 0, 0, (FLOAT)WinApp::kClientWidth, (FLOAT)WinApp::kClientHeight, 0, 1 };
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissor{ 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
    commandList_->RSSetScissorRects(1, &scissor);
}

/**
 * PreDrawToSwapChain: 描き込み先を実際のモニターへ切り替える
 */
void DirectXCommon::PreDrawToSwapChain(
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE postEffectResultSrvHandle,
    FullscreenPostEffectType postEffectType) {
    assert(renderTextureResource_);
    assert(postEffectResultResource_);
    assert(copyImageRootSignature_);
    TransitionRenderTexture(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionPostEffectResult(D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE postEffectRtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    postEffectRtvHandle.ptr += static_cast<size_t>(kPostEffectResultRTVIndex) * rtvDescriptorSize_;
    commandList_->OMSetRenderTargets(1, &postEffectRtvHandle, false, nullptr);

    float postEffectClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList_->ClearRenderTargetView(postEffectRtvHandle, postEffectClearColor, 0, nullptr);

    D3D12_VIEWPORT viewport{ 0, 0, (FLOAT)WinApp::kClientWidth, (FLOAT)WinApp::kClientHeight, 0, 1 };
    commandList_->RSSetViewports(1, &viewport);
    D3D12_RECT scissor{ 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
    commandList_->RSSetScissorRects(1, &scissor);

    DrawFullscreenTriangle(renderTextureSrvHandle, postEffectType);
    TransitionPostEffectResult(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 
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

    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);

    DrawFullscreenTriangle(postEffectResultSrvHandle, FullscreenPostEffectType::Copy);
}

void DirectXCommon::RestoreRenderTextureToRenderTarget() {
    TransitionRenderTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
}

/**
 * PostDraw: GPUへの命令送信と同期
 */
void DirectXCommon::TransitionRenderTexture(D3D12_RESOURCE_STATES stateAfter) {
    if (!renderTextureResource_ || renderTextureState_ == stateAfter) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTextureResource_.Get();
    barrier.Transition.StateBefore = renderTextureState_;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    renderTextureState_ = stateAfter;
}

void DirectXCommon::TransitionPostEffectResult(D3D12_RESOURCE_STATES stateAfter) {
    if (!postEffectResultResource_ || postEffectResultState_ == stateAfter) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = postEffectResultResource_.Get();
    barrier.Transition.StateBefore = postEffectResultState_;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    postEffectResultState_ = stateAfter;
}

void DirectXCommon::DrawFullscreenTriangle(
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
    FullscreenPostEffectType postEffectType) {
    size_t effectIndex = static_cast<size_t>(postEffectType);
    if (effectIndex >= kFullscreenPostEffectCount) {
        effectIndex = static_cast<size_t>(FullscreenPostEffectType::Copy);
    }
    assert(fullscreenPostEffectPipelineStates_[effectIndex]);
    assert(fullscreenPostEffectParameterResource_);
    assert(vignetteParameterResource_);

    commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
    commandList_->SetPipelineState(fullscreenPostEffectPipelineStates_[effectIndex].Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->SetGraphicsRootDescriptorTable(0, textureSrvHandle);
    ID3D12Resource* parameterResource =
        postEffectType == FullscreenPostEffectType::Vignette
        ? vignetteParameterResource_.Get()
        : fullscreenPostEffectParameterResource_.Get();
    commandList_->SetGraphicsRootConstantBufferView(1, parameterResource->GetGPUVirtualAddress());
    commandList_->DrawInstanced(3, 1, 0, 0);
}

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

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUAVBufferResource(size_t sizeInBytes, D3D12_RESOURCE_STATES initialState) {
    D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
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
