#pragma once
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>

class Model; // 前方宣言

class ModelCommon {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // モデル読み込み
    void LoadModel(const std::string& filename);

    // モデル取得
    Model* GetModel(const std::string& filename);

    // ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    DirectXCommon* dxCommon_ = nullptr;

    // モデルデータ格納用
    std::map<std::string, std::unique_ptr<Model>> models_;
};