#pragma once
#include "DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>

// Modelのインクルードは不要になりました

class Object3dCommon {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 共通描画設定
    void CommonDrawSettings();

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    // ★ LoadModel, GetModel は ModelCommon へ移動したので削除

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // ★ models_ マップも削除
};