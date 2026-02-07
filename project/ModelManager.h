#pragma once
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>

class Model;

class ModelManager {
public:
	void Initialize(DirectXCommon* dxCommon);
	void LoadModel(const std::string& filename);
	Model* GetModel(const std::string& filename);
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
	std::map<std::string, std::unique_ptr<Model>> models_;
};