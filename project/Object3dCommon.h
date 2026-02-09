#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h" // ★追加

class Object3dCommon {
public:
    // 初期化に SrvManager を追加
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void CommonDrawSettings();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    // ★追加: SrvManagerのゲッター
    SrvManager* GetSrvManager() const { return srvManager_; }

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr; // ★追加

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};