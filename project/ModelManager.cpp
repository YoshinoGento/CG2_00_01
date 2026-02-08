#include "ModelManager.h"
#include "Model.h"
#include <cassert>

// ★修正: SrvManagerを受け取る
void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}

void ModelManager::LoadModel(const std::string& filename) {
    if (models_.contains(filename)) {
        return;
    }

    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(this, "Resources", filename);

    models_[filename] = std::move(model);
}

Model* ModelManager::GetModel(const std::string& filename) {
    if (models_.contains(filename)) {
        return models_[filename].get();
    }
    return nullptr;
}