#include "farm/render/FarmRenderer.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmHarvestVisualSystem.h"
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
	constexpr std::array<const char*,3> cropPaths{"farm/crop_turnip.obj", "farm/crop_carrot.obj", "farm/crop_leaves.obj"};
	bool cropFilesPresent = true;
	for (const auto* path : cropPaths) cropFilesPresent &= std::filesystem::is_regular_file(std::filesystem::path("Resources") / path, error);
	if (cropFilesPresent) {
		for (std::size_t i=0; i<cropPaths.size(); ++i) {
			models->LoadModel(cropPaths[i]);
			cropModels_[i] = models->GetModel(cropPaths[i]);
			if (cropModels_[i] && cropModels_[i]->HasSkinCluster()) cropModels_[i] = nullptr;
		}
	}
	parts_.reserve(kMaximumParts + kMaximumTargetParts + kMaximumCropParts + kMaximumHarvestParts);
	objects_.reserve(kMaximumParts + kMaximumTargetParts + kMaximumCropParts + kMaximumHarvestParts);
	return true;
}

void FarmRenderer::Prepare(const FarmGrid& grid, const FarmVisualSystem& visual, Camera* camera,
	int hoveredTileIndex, const FarmHarvestVisualSystem* harvest) {
	parts_.clear();
	lastDrawTileCount_ = 0;
	limitExceeded_ = false;
	if (!IsReady() || !visible_ || !camera) { return; }
	if (grid.GetTileCount() > static_cast<int>(kMaximumParts)) { limitExceeded_ = true; return; }
	std::size_t terrainCount = 0;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const auto tileParts = BuildFarmTileMeshParts(grid, index, visual);
		if (terrainCount + tileParts.count > kMaximumParts) {
			parts_.clear(); lastDrawTileCount_ = 0; limitExceeded_ = true; return;
		}
		parts_.insert(parts_.end(), tileParts.parts.begin(), tileParts.parts.begin() + tileParts.count);
		terrainCount += tileParts.count;
		if (HasCropMeshes()) {
			const auto crops = BuildFarmCropMeshParts(grid, index, visual);
			parts_.insert(parts_.end(), crops.parts.begin(), crops.parts.begin() + crops.count);
		}
		lastDrawTileCount_ += tileParts.count > 0 ? 1 : 0;
	}
	static_assert(kMaximumHarvestParts == FarmHarvestVisualSystem::kCapacity * 2);
	if (harvest && HasCropMeshes()) {
		for (std::size_t i = 0; i < FarmHarvestVisualSystem::kCapacity; ++i) {
			const auto effect = harvest->GetParts(grid, i);
			parts_.insert(parts_.end(), effect.parts.begin(), effect.parts.begin() + effect.count);
		}
	}
	const auto selection = BuildFarmSelectionMeshParts(grid, grid.GetSelectedIndex(), visual);
	static_assert(selection.parts.size() * 2 == kMaximumTargetParts);
	parts_.insert(parts_.end(), selection.parts.begin(), selection.parts.begin() + selection.count);
	if (hoveredTileIndex != grid.GetSelectedIndex()) {
		const auto hover = BuildFarmHoverMeshParts(grid, hoveredTileIndex, visual);
		parts_.insert(parts_.end(), hover.parts.begin(), hover.parts.begin() + hover.count);
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
		if (part.shape == FarmMeshShape::Turnip) desired = cropModels_[0];
		if (part.shape == FarmMeshShape::Carrot) desired = cropModels_[1];
		if (part.shape == FarmMeshShape::Leaves) desired = cropModels_[2];
		if (object.GetModel() != desired) { object.SetModel(desired); }
		object.SetPosition(part.position);
		if (!object.SetShearY(part.slope)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		if (!object.SetScale(part.scale)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		object.SetColor(part.color);
		object.SetEnableLighting(!part.water && part.surface != FarmMeshSurface::Selection &&
			part.surface != FarmMeshSurface::Hover);
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
		if (!parts_[index].water && parts_[index].surface != FarmMeshSurface::SoilBoundary &&
			parts_[index].surface != FarmMeshSurface::Selection &&
			parts_[index].surface != FarmMeshSurface::Hover) { objects_[index]->DrawShadow(); }
	}
}

} // namespace farm
