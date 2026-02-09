#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h" // ★追加
#include <map>
#include <string>
#include <memory>

class Model;

class ModelManager {
public:
    // 初期化に SrvManager を追加
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void LoadModel(const std::string& filename);
    Model* GetModel(const std::string& filename);
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    // ★追加: ゲッター
    SrvManager* GetSrvManager() const { return srvManager_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr; // ★追加

    std::map<std::string, std::unique_ptr<Model>> models_;
};