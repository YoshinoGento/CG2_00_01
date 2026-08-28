#pragma once
#include "base/DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <string>

class SpriteCommon {
public:
    // SpriteCommon owns only the shared 2D rendering state; texture assets belong to TextureManager.
    void Initialize(DirectXCommon* dxCommon);

    // 描画前の準備
    void PreDraw();

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
    uint32_t LoadTexture(const std::string& texturePath);
    D3D12_RESOURCE_DESC GetTextureResourceDesc(uint32_t textureHandle) const;

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

};
