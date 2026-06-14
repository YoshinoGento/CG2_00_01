#include "RenderTexture.h"

#include "base/DirectXCommon.h"
#include "base/SrvManager.h"

#include <cassert>

void RenderTexture::Initialize(
    DirectXCommon* dxCommon,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor,
    D3D12_RESOURCE_STATES initialState,
    const std::string& debugName) {
    assert(dxCommon);
    assert(width > 0);
    assert(height > 0);

    width_ = width;
    height_ = height;
    format_ = format;
    clearColor_ = clearColor;
    state_ = initialState;

    D3D12_RESOURCE_DESC desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = 1;
    desc.DepthOrArraySize = 1;
    desc.Format = format_;
    desc.SampleDesc.Count = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_.x;
    clearValue.Color[1] = clearColor_.y;
    clearValue.Color[2] = clearColor_.z;
    clearValue.Color[3] = clearColor_.w;

    HRESULT hr = dxCommon->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        &clearValue,
        IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(hr));

    if (!debugName.empty()) {
        std::wstring wideName(debugName.begin(), debugName.end());
        resource_->SetName(wideName.c_str());
    }

    uint32_t rtvIndex = dxCommon->AllocateRTV();
    rtvHandle_ = dxCommon->GetRTVHandle(rtvIndex);
    dxCommon->GetDevice()->CreateRenderTargetView(resource_.Get(), nullptr, rtvHandle_);
}

void RenderTexture::CreateSRV(SrvManager* srvManager, DXGI_FORMAT srvFormat, UINT mipLevels) {
    assert(srvManager);
    assert(resource_);
    assert(mipLevels > 0);

    srvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(srvIndex_, resource_.Get(), srvFormat, mipLevels);
    srvGpuHandle_ = srvManager->GetGPUDescriptorHandle(srvIndex_);
}

void RenderTexture::Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES after) {
    assert(commandList);
    if (!resource_ || state_ == after) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = state_;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    state_ = after;
}

void RenderTexture::Clear(ID3D12GraphicsCommandList* commandList) {
    assert(commandList);
    float color[4] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
    commandList->ClearRenderTargetView(rtvHandle_, color, 0, nullptr);
}
