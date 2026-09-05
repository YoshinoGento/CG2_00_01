#include "farm/render/FarmRenderer.h"

#include "farm/core/FarmGrid.h"
#include "3d/Object3d.h"
#include "3d/ModelManager.h"
#include <filesystem>

namespace farm {

FarmRenderer::FarmRenderer() = default;
FarmRenderer::~FarmRenderer() = default;

bool FarmRenderer::Initialize(Object3dCommon* common, ModelManager* models, Texture2DHandle whiteTexture) {
	if (IsReady()) { return true; }
	std::error_code error;
	if (!common || !models || !std::filesystem::is_regular_file("Resources/farm/unit_box.obj", error)) { return false; }
	if (!std::filesystem::is_regular_file("Resources/farm/triangle_lower.obj", error) ||
		!std::filesystem::is_regular_file("Resources/farm/triangle_upper.obj", error)) { return false; }
	models->LoadModel("farm/unit_box.obj");
	Model* model = models->GetModel("farm/unit_box.obj");
	if (!model || model->HasSkinCluster()) { return false; }
	models->LoadModel("farm/triangle_lower.obj");
	models->LoadModel("farm/triangle_upper.obj");
	Model* lower = models->GetModel("farm/triangle_lower.obj");
	Model* upper = models->GetModel("farm/triangle_upper.obj");
	if (!lower || !upper || lower->HasSkinCluster() || upper->HasSkinCluster()) { return false; }
	common_ = common;
	model_ = model;
	triangleLower_ = lower;
	triangleUpper_ = upper;
	whiteTexture_ = whiteTexture;
	parts_.reserve(kMaximumParts);
	objects_.reserve(kMaximumParts);
	return true;
}

void FarmRenderer::Prepare(const FarmGrid& grid, const FarmVisualSystem& visual, Camera* camera) {
	parts_.clear();
	lastDrawTileCount_ = 0;
	limitExceeded_ = false;
	if (!IsReady() || !visible_ || !camera) { return; }
	if (grid.GetTileCount() > static_cast<int>(kMaximumParts)) { limitExceeded_ = true; return; }
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const auto tileParts = BuildFarmTileMeshParts(grid, index, visual);
		if (parts_.size() + tileParts.count > kMaximumParts) {
			parts_.clear(); lastDrawTileCount_ = 0; limitExceeded_ = true; return;
		}
		parts_.insert(parts_.end(), tileParts.parts.begin(), tileParts.parts.begin() + tileParts.count);
		lastDrawTileCount_ += tileParts.count > 0 ? 1 : 0;
	}
	// Retain unused objects until scene destruction; previous-frame GPU work is fenced by PostDraw.
	while (objects_.size() < parts_.size()) {
		auto object = std::make_unique<Object3d>();
		object->Initialize(common_);
		object->SetModel(model_);
		object->SetTexture(whiteTexture_);
		objects_.push_back(std::move(object));
	}
	for (std::size_t index = 0; index < parts_.size(); ++index) {
		const auto& part = parts_[index];
		auto& object = *objects_[index];
		Model* desired = part.shape == FarmMeshShape::TriangleLower ? triangleLower_ :
			(part.shape == FarmMeshShape::TriangleUpper ? triangleUpper_ : model_);
		if (object.GetModel() != desired) { object.SetModel(desired); }
		object.SetPosition(part.position);
		if (!object.SetShearY(part.slope)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		if (!object.SetScale(part.scale)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		object.SetColor(part.color);
		object.SetEnableLighting(!part.water);
		object.Update(camera, 0.0f);
	}
}

void FarmRenderer::Draw() {
	if (!visible_ || !IsReady()) { return; }
	for (std::size_t index = 0; index < parts_.size(); ++index) { objects_[index]->Draw(); }
}

void FarmRenderer::DrawShadow() {
	if (!visible_ || !IsReady()) { return; }
	for (std::size_t index = 0; index < parts_.size(); ++index) {
		if (!parts_[index].water) { objects_[index]->DrawShadow(); }
	}
}

} // namespace farm
