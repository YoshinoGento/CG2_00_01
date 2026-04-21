#pragma once
#include "base/DirectXCommon.h"
#include "base/SrvManager.h" // ★追加
#include <string>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d12.h>

class SpriteCommon {
public:
    // 初期化に SrvManager を追加
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 描画前の準備
    void PreDraw();

    // テクスチャ読み込み
    uint32_t LoadTexture(const std::string& filePath);

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    // ★追加: 指定番号のSRVハンドル(GPU)を取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

    // 指定番号のテクスチャ情報（幅・高さなど）を取得
    D3D12_RESOURCE_DESC GetTextureResourceDesc(uint32_t textureIndex);

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr; // ★追加

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // テクスチャデータ管理
    std::map<std::string, uint32_t> textureMap_;
    std::map<uint32_t, Microsoft::WRL::ComPtr<ID3D12Resource>> textureResources_;
    std::map<uint32_t, D3D12_RESOURCE_DESC> textureDescs_;
};