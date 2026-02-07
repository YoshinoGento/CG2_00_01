#include "ModelManager.h"
#include "Model.h"
#include <cassert>

void ModelManager::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
}

void ModelManager::LoadModel(const std::string& filename) {
	if (models_.contains(filename)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	// ModelManager (this) を渡す
	model->Initialize(this, "Resources", filename);

	models_[filename] = std::move(model);
}

Model* ModelManager::GetModel(const std::string& filename) {
	if (models_.contains(filename)) {
		return models_[filename].get();
	}
	return nullptr;
}