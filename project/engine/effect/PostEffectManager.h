#pragma once

#include "base/DirectXCommon.h"
#include "effect/FullscreenPass.h"
#include "effect/RenderTexture.h"

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

class SrvManager;

class PostEffectManager {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        uint32_t sceneSrvIndex,
        uint32_t postEffectResultSrvIndex,
        uint32_t finalDisplaySrvIndex,
        uint32_t depthSrvIndex,
        uint32_t normalSrvIndex);

    void Execute(
        DirectXCommon::FullscreenPostEffectType postEffectType,
        D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle);

    void SetChainModeEnabled(bool enabled) { chainModeEnabled_ = enabled; }
    bool IsChainModeEnabled() const { return chainModeEnabled_; }
    size_t GetChainPassCount() const { return chainPassTypes_.size(); }
    const char* GetChainPassName(size_t index) const;
    bool IsChainPassEnabled(size_t index) const;
    void SetChainPassEnabled(size_t index, bool enabled);
    size_t GetEnabledChainPassCount() const;

    uint32_t GetFinalDisplaySrvIndex() const { return finalDisplaySrvIndex_; }

private:
    static constexpr size_t kPostEffectPassCount =
        static_cast<size_t>(DirectXCommon::FullscreenPostEffectType::Count);

    struct DummyTexture {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE srv{};
    };

    void InitializePasses();
    void InitializePingPongTextures();
    void InitializeChainPasses();
    void InitializeDummyTextures();
    void CreateDummyTexture(const char* debugName, const std::array<uint8_t, 4>& color, DummyTexture& dummyTexture);
    DirectXCommon::FullscreenPostEffectType NormalizePostEffectType(
        DirectXCommon::FullscreenPostEffectType postEffectType) const;
    PostEffectPassDesc CreatePassDesc(
        DirectXCommon::FullscreenPostEffectType postEffectType,
        D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle);
    D3D12_GPU_VIRTUAL_ADDRESS GetParameterAddress(
        DirectXCommon::FullscreenPostEffectType postEffectType) const;
    const char* GetPassName(DirectXCommon::FullscreenPostEffectType postEffectType) const;
    bool RequiresDepthTexture(DirectXCommon::FullscreenPostEffectType postEffectType) const;
    bool RequiresNormalTexture(DirectXCommon::FullscreenPostEffectType postEffectType) const;
    void BuildActivePasses(D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle);
    void DrawPass(D3D12_GPU_DESCRIPTOR_HANDLE inputSrv, const PostEffectPassDesc& passDesc) const;
    void DrawToRenderTexture(RenderTexture& outputTexture, D3D12_GPU_DESCRIPTOR_HANDLE inputSrv, const PostEffectPassDesc& passDesc);
    void ExecuteSingle(
        DirectXCommon::FullscreenPostEffectType postEffectType,
        D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle);
    void ExecuteChain(D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle);
    void FinishToDisplay();
    void CopyTextureToPostEffectResult(D3D12_GPU_DESCRIPTOR_HANDLE inputSrv);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::array<FullscreenPass, kPostEffectPassCount> passes_;
    RenderTexture pingTexture_;
    RenderTexture pongTexture_;
    bool chainModeEnabled_ = false;
    std::vector<DirectXCommon::FullscreenPostEffectType> chainPassTypes_;
    std::vector<bool> chainPassEnabled_;
    std::vector<PostEffectPassDesc> activePasses_;
    DummyTexture whiteDummyTexture_;
    DummyTexture blackDummyTexture_;
    DummyTexture flatNormalDummyTexture_;
    uint32_t sceneSrvIndex_ = 0;
    uint32_t postEffectResultSrvIndex_ = 0;
    uint32_t finalDisplaySrvIndex_ = 0;
    uint32_t depthSrvIndex_ = 0;
    uint32_t normalSrvIndex_ = 0;
};
