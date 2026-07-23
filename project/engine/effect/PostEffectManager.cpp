#include "PostEffectManager.h"

#include "base/SrvManager.h"
#include "2d/TextureManager.h"

#include <algorithm>
#include <array>
#include <cassert>

void PostEffectManager::InitializePasses() {
    assert(dxCommon_);

    struct PassInit {
        DirectXCommon::FullscreenPostEffectType type;
        const wchar_t* pixelShaderPath;
    };

    const PassInit passInits[] = {
        { DirectXCommon::FullscreenPostEffectType::Copy, L"Resources/shader/CopyImage.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::Grayscale, L"Resources/shader/Grayscale.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::Sepia, L"Resources/shader/Sepia.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::Blur, L"Resources/shader/Blur.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::Bloom, L"Resources/shader/Bloom.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::BoxFilter3x3, L"Resources/shader/BoxFilter.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::BoxFilter5x5, L"Resources/shader/BoxFilter5x5.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::RadialBlur, L"Resources/shader/RadialBlur.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::Dissolve, L"Resources/shader/Dissolve.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::OutlineLuminance, L"Resources/shader/OutlineLuminance.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::OutlineDepth, L"Resources/shader/OutlineDepth.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::OutlineNormal, L"Resources/shader/OutlineNormal.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::OutlineDepthNormal, L"Resources/shader/OutlineDepthNormal.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::Vignette, L"Resources/shader/Vignette.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::RandomNoise, L"Resources/shader/RandomNoise.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::HSVFilter, L"Resources/shader/HSVFilter.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::GaussianFilter, L"Resources/shader/GaussianFilter.PS.hlsl" },
        { DirectXCommon::FullscreenPostEffectType::LinearToSRGB, L"Resources/shader/LinearToSRGB.PS.hlsl" },
    };

    for (const PassInit& passInit : passInits) {
        const size_t index = static_cast<size_t>(passInit.type);
        assert(index < passes_.size());
        passes_[index].Initialize(dxCommon_, passInit.pixelShaderPath, DXGI_FORMAT_R8G8B8A8_UNORM);
    }
}

void PostEffectManager::InitializePingPongTextures() {
    assert(dxCommon_);
    assert(srvManager_);
    assert(dxCommon_->GetRenderTextureResource());

    const D3D12_RESOURCE_DESC sceneDesc = dxCommon_->GetRenderTextureResource()->GetDesc();
    const uint32_t width = static_cast<uint32_t>(sceneDesc.Width);
    const uint32_t height = static_cast<uint32_t>(sceneDesc.Height);
    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    const Vector4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    pingTexture_.Initialize(
        dxCommon_,
        width,
        height,
        format,
        clearColor,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        "PostEffectPingTexture");
    pingTexture_.CreateSRV(srvManager_, format, 1);

    pongTexture_.Initialize(
        dxCommon_,
        width,
        height,
        format,
        clearColor,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        "PostEffectPongTexture");
    pongTexture_.CreateSRV(srvManager_, format, 1);
}

void PostEffectManager::InitializeChainPasses() {
    chainPassTypes_ = {
        DirectXCommon::FullscreenPostEffectType::Grayscale,
        DirectXCommon::FullscreenPostEffectType::Sepia,
        DirectXCommon::FullscreenPostEffectType::HSVFilter,
        DirectXCommon::FullscreenPostEffectType::Vignette,
        DirectXCommon::FullscreenPostEffectType::BoxFilter3x3,
        DirectXCommon::FullscreenPostEffectType::GaussianFilter,
        DirectXCommon::FullscreenPostEffectType::RadialBlur,
        DirectXCommon::FullscreenPostEffectType::RandomNoise,
        DirectXCommon::FullscreenPostEffectType::Dissolve,
        DirectXCommon::FullscreenPostEffectType::OutlineDepth,
        DirectXCommon::FullscreenPostEffectType::OutlineNormal,
    };
    chainPassEnabled_.assign(chainPassTypes_.size(), false);
    activePasses_.clear();
    activePasses_.reserve(chainPassTypes_.size());
}

void PostEffectManager::CreateDummyTexture(
    const char* debugName,
    const std::array<uint8_t, 4>& color,
    DummyTexture& dummyTexture) {
    assert(dxCommon_);
    assert(srvManager_);
    assert(debugName);

    dummyTexture.handle = TextureManager::GetInstance()->CreateSolidColorTexture2D(debugName, color);
    dummyTexture.srv = TextureManager::GetInstance()->GetGpuHandle(dummyTexture.handle);
}

void PostEffectManager::InitializeDummyTextures() {
    CreateDummyTexture("PostEffectWhiteDummyTexture", { 255, 255, 255, 255 }, whiteDummyTexture_);
    CreateDummyTexture("PostEffectBlackDummyTexture", { 0, 0, 0, 255 }, blackDummyTexture_);
    CreateDummyTexture("PostEffectFlatNormalDummyTexture", { 128, 128, 255, 255 }, flatNormalDummyTexture_);
}

DirectXCommon::FullscreenPostEffectType PostEffectManager::NormalizePostEffectType(
    DirectXCommon::FullscreenPostEffectType postEffectType) const {
    const size_t index = static_cast<size_t>(postEffectType);
    if (index >= passes_.size()) {
        return DirectXCommon::FullscreenPostEffectType::Copy;
    }
    return postEffectType;
}

const char* PostEffectManager::GetChainPassName(size_t index) const {
    if (index >= chainPassTypes_.size()) {
        return "";
    }
    return GetPassName(chainPassTypes_[index]);
}

bool PostEffectManager::IsChainPassEnabled(size_t index) const {
    if (index >= chainPassEnabled_.size()) {
        return false;
    }
    return chainPassEnabled_[index];
}

void PostEffectManager::SetChainPassEnabled(size_t index, bool enabled) {
    if (index >= chainPassEnabled_.size()) {
        return;
    }
    chainPassEnabled_[index] = enabled;
}

size_t PostEffectManager::GetEnabledChainPassCount() const {
    size_t enabledCount = 0;
    for (bool enabled : chainPassEnabled_) {
        if (enabled) {
            ++enabledCount;
        }
    }
    return enabledCount;
}

const char* PostEffectManager::GetPassName(DirectXCommon::FullscreenPostEffectType postEffectType) const {
    switch (postEffectType) {
    case DirectXCommon::FullscreenPostEffectType::Copy:
        return "Copy";
    case DirectXCommon::FullscreenPostEffectType::Grayscale:
        return "Grayscale";
    case DirectXCommon::FullscreenPostEffectType::Sepia:
        return "Sepia";
    case DirectXCommon::FullscreenPostEffectType::Blur:
        return "Blur";
    case DirectXCommon::FullscreenPostEffectType::Bloom:
        return "Bloom";
    case DirectXCommon::FullscreenPostEffectType::BoxFilter3x3:
        return "BoxFilter";
    case DirectXCommon::FullscreenPostEffectType::BoxFilter5x5:
        return "BoxFilter5x5";
    case DirectXCommon::FullscreenPostEffectType::RadialBlur:
        return "RadialBlur";
    case DirectXCommon::FullscreenPostEffectType::Dissolve:
        return "Dissolve";
    case DirectXCommon::FullscreenPostEffectType::OutlineLuminance:
        return "OutlineLuminance";
    case DirectXCommon::FullscreenPostEffectType::OutlineDepth:
        return "OutlineDepth";
    case DirectXCommon::FullscreenPostEffectType::OutlineNormal:
        return "OutlineNormal";
    case DirectXCommon::FullscreenPostEffectType::OutlineDepthNormal:
        return "OutlineDepthNormal";
    case DirectXCommon::FullscreenPostEffectType::Vignette:
        return "Vignette";
    case DirectXCommon::FullscreenPostEffectType::RandomNoise:
        return "RandomNoise";
    case DirectXCommon::FullscreenPostEffectType::HSVFilter:
        return "HSVFilter";
    case DirectXCommon::FullscreenPostEffectType::GaussianFilter:
        return "GaussianFilter";
    case DirectXCommon::FullscreenPostEffectType::LinearToSRGB:
        return "LinearToSRGB";
    default:
        return "Copy";
    }
}

D3D12_GPU_VIRTUAL_ADDRESS PostEffectManager::GetParameterAddress(
    DirectXCommon::FullscreenPostEffectType postEffectType) const {
    assert(dxCommon_);

    switch (postEffectType) {
    case DirectXCommon::FullscreenPostEffectType::Vignette:
        return dxCommon_->GetVignetteParameterAddress();
    case DirectXCommon::FullscreenPostEffectType::RadialBlur:
        return dxCommon_->GetRadialBlurParameterAddress();
    case DirectXCommon::FullscreenPostEffectType::Dissolve:
        return dxCommon_->GetDissolveParameterAddress();
    case DirectXCommon::FullscreenPostEffectType::RandomNoise:
        return dxCommon_->GetRandomNoiseParameterAddress();
    case DirectXCommon::FullscreenPostEffectType::HSVFilter:
        return dxCommon_->GetHSVFilterParameterAddress();
    default:
        return dxCommon_->GetCommonPostEffectParameterAddress();
    }
}

bool PostEffectManager::RequiresDepthTexture(DirectXCommon::FullscreenPostEffectType postEffectType) const {
    return postEffectType == DirectXCommon::FullscreenPostEffectType::OutlineDepth ||
        postEffectType == DirectXCommon::FullscreenPostEffectType::OutlineDepthNormal;
}

bool PostEffectManager::RequiresNormalTexture(DirectXCommon::FullscreenPostEffectType postEffectType) const {
    return postEffectType == DirectXCommon::FullscreenPostEffectType::OutlineNormal ||
        postEffectType == DirectXCommon::FullscreenPostEffectType::OutlineDepthNormal;
}

PostEffectPassDesc PostEffectManager::CreatePassDesc(
    DirectXCommon::FullscreenPostEffectType postEffectType,
    D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle) {
    assert(srvManager_);

    postEffectType = NormalizePostEffectType(postEffectType);
    const size_t index = static_cast<size_t>(postEffectType);

    PostEffectPassDesc desc{};
    desc.name = GetPassName(postEffectType);
    desc.pass = &passes_[index];
    desc.enabled = true;
    desc.cbv = GetParameterAddress(postEffectType);
    desc.useCBV = desc.cbv != 0;
    desc.srvT1 = blackDummyTexture_.srv;
    desc.srvT2 = flatNormalDummyTexture_.srv;

    if (RequiresDepthTexture(postEffectType)) {
        desc.srvT1 = srvManager_->GetGPUDescriptorHandle(depthSrvIndex_);
        desc.useSrvT1 = true;
    } else if (postEffectType == DirectXCommon::FullscreenPostEffectType::Dissolve) {
        desc.srvT1 = auxiliarySrvHandle.ptr != 0 ? auxiliarySrvHandle : whiteDummyTexture_.srv;
        desc.useSrvT1 = true;
    }

    if (RequiresNormalTexture(postEffectType)) {
        desc.srvT2 = srvManager_->GetGPUDescriptorHandle(normalSrvIndex_);
        desc.useSrvT2 = true;
    }

    return desc;
}

void PostEffectManager::BuildActivePasses(D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle) {
    activePasses_.clear();
    activePasses_.reserve(chainPassTypes_.size());

    for (size_t i = 0; i < chainPassTypes_.size(); ++i) {
        PostEffectPassDesc passDesc = CreatePassDesc(chainPassTypes_[i], auxiliarySrvHandle);
        passDesc.enabled = i < chainPassEnabled_.size() ? chainPassEnabled_[i] : false;
        activePasses_.push_back(passDesc);
    }
}

void PostEffectManager::DrawPass(D3D12_GPU_DESCRIPTOR_HANDLE inputSrv, const PostEffectPassDesc& passDesc) const {
    if (!passDesc.enabled) {
        return;
    }

    assert(dxCommon_);
    assert(passDesc.pass);
    assert(passDesc.pass->IsInitialized());
    assert(inputSrv.ptr != 0);
    assert(passDesc.useCBV);
    assert(passDesc.cbv != 0);

    passDesc.pass->Draw(dxCommon_->GetCommandList(), inputSrv, passDesc);
}

void PostEffectManager::DrawToRenderTexture(
    RenderTexture& outputTexture,
    D3D12_GPU_DESCRIPTOR_HANDLE inputSrv,
    const PostEffectPassDesc& passDesc) {
    assert(dxCommon_);
    assert(inputSrv.ptr != 0);
    assert(outputTexture.GetResource());

    outputTexture.Transition(dxCommon_->GetCommandList(), D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = outputTexture.GetRTV();
    dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    outputTexture.Clear(dxCommon_->GetCommandList());

    D3D12_VIEWPORT viewport{
        0.0f,
        0.0f,
        static_cast<FLOAT>(outputTexture.GetWidth()),
        static_cast<FLOAT>(outputTexture.GetHeight()),
        0.0f,
        1.0f,
    };
    D3D12_RECT scissor{
        0,
        0,
        static_cast<LONG>(outputTexture.GetWidth()),
        static_cast<LONG>(outputTexture.GetHeight()),
    };
    dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);
    dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissor);

    srvManager_->PreDraw();
    DrawPass(inputSrv, passDesc);
    outputTexture.Transition(dxCommon_->GetCommandList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void PostEffectManager::CopyTextureToPostEffectResult(D3D12_GPU_DESCRIPTOR_HANDLE inputSrv) {
    const PostEffectPassDesc copyPass = CreatePassDesc(DirectXCommon::FullscreenPostEffectType::Copy, {});
    dxCommon_->BeginPostEffectResultRenderTarget();
    srvManager_->PreDraw();
    DrawPass(inputSrv, copyPass);
    dxCommon_->TransitionPostEffectResult(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void PostEffectManager::FinishToDisplay() {
    const D3D12_GPU_DESCRIPTOR_HANDLE postEffectResultSrv =
        srvManager_->GetGPUDescriptorHandle(postEffectResultSrvIndex_);
    const D3D12_GPU_DESCRIPTOR_HANDLE finalDisplaySrv =
        srvManager_->GetGPUDescriptorHandle(finalDisplaySrvIndex_);

    const PostEffectPassDesc gammaPass =
        CreatePassDesc(DirectXCommon::FullscreenPostEffectType::LinearToSRGB, {});
    dxCommon_->BeginFinalDisplayRenderTarget();
    srvManager_->PreDraw();
    DrawPass(postEffectResultSrv, gammaPass);
    dxCommon_->TransitionFinalDisplayTexture(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    const PostEffectPassDesc copyPass =
        CreatePassDesc(DirectXCommon::FullscreenPostEffectType::Copy, {});
    dxCommon_->BeginSwapChainRenderTarget();
    srvManager_->PreDraw();
    DrawPass(finalDisplaySrv, copyPass);
}

void PostEffectManager::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    uint32_t sceneSrvIndex,
    uint32_t postEffectResultSrvIndex,
    uint32_t finalDisplaySrvIndex,
    uint32_t depthSrvIndex,
    uint32_t normalSrvIndex) {
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    sceneSrvIndex_ = sceneSrvIndex;
    postEffectResultSrvIndex_ = postEffectResultSrvIndex;
    finalDisplaySrvIndex_ = finalDisplaySrvIndex;
    depthSrvIndex_ = depthSrvIndex;
    normalSrvIndex_ = normalSrvIndex;

    InitializeDummyTextures();
    InitializePasses();
    InitializePingPongTextures();
    InitializeChainPasses();
}

void PostEffectManager::Finalize() {
	pingTexture_.Finalize(srvManager_);
	pongTexture_.Finalize(srvManager_);
	activePasses_.clear();
	chainPassEnabled_.clear();
	chainPassTypes_.clear();
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
}

void PostEffectManager::ExecuteSingle(
    DirectXCommon::FullscreenPostEffectType postEffectType,
    D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle) {
    assert(dxCommon_);
    assert(srvManager_);

    const D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv = srvManager_->GetGPUDescriptorHandle(sceneSrvIndex_);

    postEffectType = NormalizePostEffectType(postEffectType);
    const PostEffectPassDesc selectedPass = CreatePassDesc(postEffectType, auxiliarySrvHandle);
    const bool requiresDepth = selectedPass.useSrvT1 && RequiresDepthTexture(postEffectType);
    const bool requiresNormal = selectedPass.useSrvT2 && RequiresNormalTexture(postEffectType);

    dxCommon_->TransitionRenderTexture(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (requiresDepth) {
        dxCommon_->TransitionDepthBuffer(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (requiresNormal) {
        dxCommon_->TransitionNormalTexture(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    dxCommon_->BeginPostEffectResultRenderTarget();
    srvManager_->PreDraw();
    DrawPass(sceneSrv, selectedPass);
    dxCommon_->TransitionPostEffectResult(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    FinishToDisplay();
}

void PostEffectManager::ExecuteChain(D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle) {
    assert(dxCommon_);
    assert(srvManager_);

    D3D12_GPU_DESCRIPTOR_HANDLE currentInputSrv = srvManager_->GetGPUDescriptorHandle(sceneSrvIndex_);
    RenderTexture* currentOutput = &pingTexture_;
    bool wroteAnyPass = false;

    dxCommon_->TransitionRenderTexture(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pingTexture_.Transition(dxCommon_->GetCommandList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pongTexture_.Transition(dxCommon_->GetCommandList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    BuildActivePasses(auxiliarySrvHandle);

    for (const PostEffectPassDesc& passDesc : activePasses_) {
        if (!passDesc.enabled) {
            continue;
        }

        if (passDesc.useSrvT1 && passDesc.srvT1.ptr == srvManager_->GetGPUDescriptorHandle(depthSrvIndex_).ptr) {
            dxCommon_->TransitionDepthBuffer(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (passDesc.useSrvT2 && passDesc.srvT2.ptr == srvManager_->GetGPUDescriptorHandle(normalSrvIndex_).ptr) {
            dxCommon_->TransitionNormalTexture(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        assert(currentInputSrv.ptr != currentOutput->GetSRV().ptr);
        DrawToRenderTexture(*currentOutput, currentInputSrv, passDesc);
        currentInputSrv = currentOutput->GetSRV();
        currentOutput = currentOutput == &pingTexture_ ? &pongTexture_ : &pingTexture_;
        wroteAnyPass = true;
    }

    CopyTextureToPostEffectResult(wroteAnyPass ? currentInputSrv : srvManager_->GetGPUDescriptorHandle(sceneSrvIndex_));
    FinishToDisplay();
}

void PostEffectManager::Execute(
    DirectXCommon::FullscreenPostEffectType postEffectType,
    D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle) {
    if (chainModeEnabled_) {
        ExecuteChain(auxiliarySrvHandle);
        return;
    }

    ExecuteSingle(postEffectType, auxiliarySrvHandle);
}
