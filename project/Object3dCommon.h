#pragma once
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>

class Model; // 前方宣言 (Model.hをインクルードすると循環参照になるため)

class Object3dCommon {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // 共通描画設定
    void CommonDrawSettings();

    // ★追加: モデル読み込み（今はダミーですが、将来ここでファイルを読みます）
    void LoadModel(const std::string& filename);

    // ★追加: 読み込み済みモデルの取得
    Model* GetModel(const std::string& filename);

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // ★追加: モデルデータ格納用 (ファイル名 -> Modelクラス)
    std::map<std::string, std::unique_ptr<Model>> models_;
};