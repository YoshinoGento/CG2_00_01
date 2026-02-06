#include "ModelCommon.h"
#include "Model.h"
#include <cassert>

void ModelCommon::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
}

void ModelCommon::LoadModel(const std::string& filename) {
    // 重複読み込み防止
    if (models_.contains(filename)) {
        return;
    }

    // モデル生成
    std::unique_ptr<Model> model = std::make_unique<Model>();

    // ModelCommon (this) を渡して初期化
    model->Initialize(this, "Resources", filename);

    // マップに登録
    models_[filename] = std::move(model);
}

Model* ModelCommon::GetModel(const std::string& filename) {
    if (models_.contains(filename)) {
        return models_[filename].get();
    }
    return nullptr;
}