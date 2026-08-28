#include "editor/GamePlayEditorBridge.h"

#include "3d/Object3d.h"
#include "base/FrameClock.h"
#include "base/Framework.h"
#include "effect/ParticleManager.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolActionSystem.h"
#include "scene/GamePlayScene.h"

#include <algorithm>
#include <string_view>

namespace editor {
namespace {
constexpr float kDirectionEpsilon = 0.0001f;

Vector3 NormalizeOrKeep(const Vector3& candidate, const Vector3& fallback) {
	return MatrixMath::Length(candidate) > kDirectionEpsilon
		? MatrixMath::Normalize(candidate)
		: fallback;
}

EditorGpuParticleMode ToEditorGpuParticleMode(GamePlayScene::GPUParticleDebugMode mode) {
	switch (mode) {
	case GamePlayScene::GPUParticleDebugMode::Agriculture:
		return EditorGpuParticleMode::Agriculture;
	case GamePlayScene::GPUParticleDebugMode::Interaction:
		return EditorGpuParticleMode::Interaction;
	case GamePlayScene::GPUParticleDebugMode::Off:
	default:
		return EditorGpuParticleMode::Off;
	}
}

GamePlayScene::GPUParticleDebugMode ToSceneGpuParticleMode(EditorGpuParticleMode mode) {
	switch (mode) {
	case EditorGpuParticleMode::Agriculture:
		return GamePlayScene::GPUParticleDebugMode::Agriculture;
	case EditorGpuParticleMode::Interaction:
		return GamePlayScene::GPUParticleDebugMode::Interaction;
	case EditorGpuParticleMode::Off:
	default:
		return GamePlayScene::GPUParticleDebugMode::Off;
	}
}

GamePlayScene::AgricultureParticleType ToSceneAgricultureParticleType(
	EditorAgricultureParticleType type) {
	switch (type) {
	case EditorAgricultureParticleType::WaterSplash:
		return GamePlayScene::AgricultureParticleType::WaterSplash;
	case EditorAgricultureParticleType::HarvestSparkle:
		return GamePlayScene::AgricultureParticleType::HarvestSparkle;
	case EditorAgricultureParticleType::DirtDust:
	default:
		return GamePlayScene::AgricultureParticleType::DirtDust;
	}
}
} // namespace

void GamePlayEditorBridge::Bind(
	GamePlayScene& scene,
	farm::FarmGrid& farmGrid,
	FarmToolActionSystem& farmToolActionSystem,
	farm::FarmIrrigationSystem& farmIrrigationSystem,
	FarmDocumentSystem& farmDocumentSystem) noexcept {
	scene_ = &scene;
	farmGrid_ = &farmGrid;
	farmToolActionSystem_ = &farmToolActionSystem;
	farmIrrigationSystem_ = &farmIrrigationSystem;
	farmDocumentSystem_ = &farmDocumentSystem;
}

void GamePlayEditorBridge::Unbind() noexcept {
	scene_ = nullptr;
	farmGrid_ = nullptr;
	farmToolActionSystem_ = nullptr;
	farmIrrigationSystem_ = nullptr;
	farmDocumentSystem_ = nullptr;
}

bool GamePlayEditorBridge::IsBound() const noexcept {
	return scene_ != nullptr && farmGrid_ != nullptr &&
		farmToolActionSystem_ != nullptr && farmIrrigationSystem_ != nullptr &&
		farmDocumentSystem_ != nullptr;
}

void GamePlayEditorBridge::BuildViewModel(GamePlayEditorViewModel& output) const {
	output.farmWidth = 0;
	output.farmHeight = 0;
	output.selectedFarmTileIndex = -1;
	output.farmGeneration = 0;
	output.farmDocumentDirty = false;
	output.farmDocumentExists = false;
	output.farmDocumentHasError = false;
	output.farmDocumentStatus = FarmDocumentStatus::Ready;
	output.farmDocumentActiveId.clear();
	output.farmDocumentName.clear();
	output.farmDocumentMessage.clear();
	output.farmDocuments.clear();
	output.farmTiles.clear();
	output.canUndo = false;
	output.canRedo = false;
	output.undoCount = 0;
	output.redoCount = 0;
	output.undoName.clear();
	output.redoName.clear();
	output.currentFarmTool = FarmTool::Hoe;
	output.selectedFarmAction = {};
	output.farmPlaytest = {};
	output.visibility = {};
	output.camera = {};
	output.objectInspector.directionalLight = {};
	output.objectInspector.spotLight = {};
	output.objectInspector.sphere = {};
	output.objectInspector.animation = {};
	output.objectInspector.cylinder = {};
	output.objectInspector.joints.clear();
	output.particles = {};
	output.simulation = {};
	if (!IsBound()) {
		return;
	}

	output.farmWidth = farmGrid_->GetWidth();
	output.farmHeight = farmGrid_->GetHeight();
	output.selectedFarmTileIndex = farmGrid_->GetSelectedIndex();
	output.farmGeneration = farmGrid_->GetGeneration();
	output.farmDocumentDirty = farmDocumentSystem_->IsDirty();
	output.farmDocumentExists = farmDocumentSystem_->FileExists();
	output.farmDocumentHasError = farmDocumentSystem_->HasError();
	output.farmDocumentStatus = farmDocumentSystem_->GetStatus();
	output.farmDocumentActiveId = farmDocumentSystem_->GetActiveDocumentId();
	output.farmDocumentName = farmDocumentSystem_->GetDisplayName();
	output.farmDocumentMessage = farmDocumentSystem_->GetStatusMessage();
	const std::vector<FarmDocumentEntry>& documents = farmDocumentSystem_->GetDocuments();
	output.farmDocuments.resize(documents.size());
	for (std::size_t index = 0; index < documents.size(); ++index) {
		const FarmDocumentEntry& source = documents[index];
		FarmDocumentEditorViewData& destination = output.farmDocuments[index];
		destination.id = source.id;
		destination.displayName = source.displayName;
		destination.savedAt = source.savedAt;
		destination.active = source.id == output.farmDocumentActiveId;
	}
	output.farmTiles.resize(static_cast<std::size_t>(farmGrid_->GetTileCount()));
	for (int index = 0; index < farmGrid_->GetTileCount(); ++index) {
		const farm::FarmTile* tile = farmGrid_->GetTile(index);
		FarmTileEditorViewData& destination = output.farmTiles[static_cast<std::size_t>(index)];
		destination = {};
		destination.index = index;
		destination.column = output.farmWidth > 0 ? index % output.farmWidth : 0;
		destination.row = output.farmWidth > 0 ? index / output.farmWidth : 0;
		if (tile != nullptr) {
			const farm::CropType selectedCrop =
				scene_->farmCropSelectionSystem_.GetSelectedCrop();
			destination.heightLevel = tile->heightLevel;
			destination.feature = tile->feature;
			destination.state = tile->state;
			destination.crop = tile->crop;
			destination.growthStage = farm::GetCropGrowthStage(*tile);
			destination.moisture = tile->moisture;
			destination.growth = tile->growth;
			destination.growthForecast = scene_->farmGrowthSystem_.Evaluate(
				*tile, selectedCrop, scene_->farmDateSystem_.GetTimeScale());
			destination.quality = farmToolActionSystem_->EvaluateHarvestQuality(*tile);
			destination.canHoe = farmToolActionSystem_->EvaluateTool(
				*farmGrid_, index, FarmTool::Hoe, selectedCrop).Succeeded();
			destination.canWater = farmToolActionSystem_->EvaluateTool(
				*farmGrid_, index, FarmTool::Water, selectedCrop).Succeeded();
			destination.canSeed = farmToolActionSystem_->EvaluateTool(
				*farmGrid_, index, FarmTool::Seed, selectedCrop,
				&scene_->farmEconomySystem_).Succeeded();
			destination.canHarvest = farmToolActionSystem_->EvaluateTool(
				*farmGrid_, index, FarmTool::Harvest, selectedCrop).Succeeded();
			destination.canToggleCanal =
				farmToolActionSystem_->CanToggleCanal(*farmGrid_, index);
			destination.canToggleWaterSource =
				farmToolActionSystem_->CanToggleWaterSource(*farmGrid_, index);
			destination.irrigationSupplied = farmIrrigationSystem_->IsSupplied(index);
		}
	}

	const CommandHistory& history = farmToolActionSystem_->GetHistory();
	output.canUndo = history.CanUndo();
	output.canRedo = history.CanRedo();
	output.undoCount = history.GetUndoCount();
	output.redoCount = history.GetRedoCount();
	if (output.canUndo) {
		const std::string_view undoName = history.GetUndoName();
		output.undoName.assign(undoName.data(), undoName.size());
	}
	if (output.canRedo) {
		const std::string_view redoName = history.GetRedoName();
		output.redoName.assign(redoName.data(), redoName.size());
	}
	output.currentFarmTool = scene_->farmToolSystem_.GetCurrentTool();
	const farm::CropType selectedCrop =
		scene_->farmCropSelectionSystem_.GetSelectedCrop();
	output.selectedFarmAction = farmToolActionSystem_->EvaluateTool(
		*farmGrid_, output.currentFarmTool, selectedCrop,
		&scene_->farmEconomySystem_);
	output.farmPlaytest.money = scene_->farmEconomySystem_.GetMoney();
	output.farmPlaytest.targetMoney = scene_->farmProgressionSystem_.GetTargetMoney();
	output.farmPlaytest.remainingMoney = scene_->farmProgressionSystem_.GetRemainingMoney(
		output.farmPlaytest.money);
	output.farmPlaytest.cropCount = scene_->farmEconomySystem_.GetTotalCropCount();
	output.farmPlaytest.cropSellPrice = scene_->farmEconomySystem_.GetSellPrice(selectedCrop);
	output.farmPlaytest.saleValue = scene_->farmEconomySystem_.GetSalePreviewValue();
	output.farmPlaytest.selectedSeedCrop = selectedCrop;
	output.farmPlaytest.seedCount = scene_->farmEconomySystem_.GetSeedCount(selectedCrop);
	output.farmPlaytest.seedPrice = scene_->farmEconomySystem_.GetSeedPrice(selectedCrop);
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		const std::size_t index = static_cast<std::size_t>(slot);
		const farm::CropType crop = farm::CropTypeFromSlot(slot);
		output.farmPlaytest.cropCounts[index] =
			scene_->farmEconomySystem_.GetCropCount(crop);
		output.farmPlaytest.cropValues[index] =
			scene_->farmEconomySystem_.GetCropInventoryValue(crop);
		output.farmPlaytest.seedCounts[index] =
			scene_->farmEconomySystem_.GetSeedCount(crop);
	}
	output.farmPlaytest.requiredCropCount = scene_->farmProgressionSystem_.GetRequiredCropCount(
		output.farmPlaytest.money, output.farmPlaytest.cropSellPrice);
	output.farmPlaytest.progress = scene_->farmProgressionSystem_.GetProgress(
		output.farmPlaytest.money);
	output.farmPlaytest.cleared = scene_->farmProgressionSystem_.IsCleared();
	output.farmPlaytest.inputLocked = output.farmPlaytest.cleared;
	output.farmPlaytest.feedbackMessage = scene_->farmFeedbackSystem_.GetCurrentMessage();
	output.farmPlaytest.lastFeedbackKind = scene_->farmFeedbackSystem_.GetLastKind();
	output.farmPlaytest.feedbackStats = scene_->farmFeedbackSystem_.GetStats();
	output.farmPlaytest.restartCount = scene_->farmRestartCount_;
	output.farmPlaytest.lastHarvestQuality =
		scene_->farmEconomySystem_.GetLastHarvestQuality();

	output.visibility.selectedTarget = scene_->selectedTarget_;
	output.visibility.showTerrain = scene_->showTerrain_;
	output.visibility.showSphere = scene_->showSphere_;
	output.visibility.showPlane = scene_->showPlane_;
	output.visibility.showSprite = scene_->showSprite_;
	output.visibility.showParticles = scene_->showParticles_;
	output.visibility.showAnimatedModel = scene_->showAnimModel_;
	output.visibility.showSkeleton = scene_->showSkeleton_;
	output.visibility.showLevelObjects = scene_->showLevelObjects_;
	output.visibility.showLevelGizmos = scene_->showLevelGizmos_;
	output.visibility.showCollisionGizmos = scene_->showLevelCollisionGizmos_;
	output.visibility.cullMode = scene_->cullMode_;
	output.camera = {
		scene_->cameraPos_,
		scene_->cameraRot_,
		scene_->cameraMoveSpeed_,
		scene_->cameraRotateSpeed_,
	};

	ObjectInspectorEditorViewData& objectView = output.objectInspector;
	objectView.directionalLight = {
		scene_->lightDirection_, scene_->lightColor_, scene_->lightIntensity_ };
	objectView.spotLight = {
		scene_->spotLightColor_,
		scene_->spotLightPos_,
		scene_->spotLightIntensity_,
		scene_->spotLightDir_,
		scene_->spotLightDistance_,
		scene_->spotLightDecay_,
		scene_->spotLightAngle_,
		scene_->spotLightFalloff_,
	};
	objectView.sphere = { scene_->sphereRadius_, scene_->spherePos_, scene_->objectRot_ };
	objectView.animation.modelIndex = scene_->currentAnimModelIdx_;
	objectView.animation.showModel = scene_->showAnimModel_;
	objectView.animation.hasObject = scene_->animObj_ != nullptr;
	if (scene_->animObj_) {
		objectView.animation.playing = scene_->animObj_->GetIsAnimationPlaying();
		objectView.animation.speed = scene_->animObj_->GetAnimationSpeed();
		objectView.animation.time = scene_->animObj_->GetAnimationTime();
		objectView.animation.duration = scene_->animObj_->GetAnimation().duration;
		std::optional<Skeleton>& skeleton = scene_->animObj_->GetSkeleton();
		if (skeleton.has_value()) {
			objectView.joints.resize(skeleton->joints.size());
			for (std::size_t index = 0; index < skeleton->joints.size(); ++index) {
				const Joint& joint = skeleton->joints[index];
				JointEditorViewData& destination = objectView.joints[index];
				destination.name = joint.name;
				destination.jointIndex = static_cast<int>(index);
				destination.translate = joint.transform.translate;
				destination.scale = joint.transform.scale;
			}
		}
	}
	objectView.cylinder = {
		scene_->cylTopRadius_,
		scene_->cylBottomRadius_,
		scene_->cylHeight_,
		scene_->cylSegments_,
		scene_->cylVertDivisions_,
	};

	output.particles.activeParticleType = scene_->activeParticleType_;
	output.particles.gpuMode = ToEditorGpuParticleMode(scene_->gpuParticleDebugMode_);
	output.particles.agriculture = {
		scene_->agricultureEmitPosition_,
		scene_->agricultureParticleSize_,
		scene_->agricultureParticleCount_,
		scene_->agricultureShowKeyGuide_,
	};
	output.particles.interaction = {
		scene_->interactionGridCount_,
		scene_->interactionParticleSize_,
		scene_->interactionBrushRadius_,
		scene_->interactionBrushStrength_,
		scene_->interactionDamping_,
		scene_->interactionBrushPosition_,
		scene_->interactionParticleCount_,
	};
	if (scene_->framework_ && scene_->framework_->GetParticleManager()) {
		output.particles.alphaReference = scene_->framework_->GetParticleManager()->GetAlphaReference();
	}
	if (scene_->framework_ && scene_->framework_->GetFrameClock()) {
		const FrameClock* frameClock = scene_->framework_->GetFrameClock();
		output.simulation.paused = frameClock->IsPaused();
		output.simulation.simulationTick = frameClock->GetSimulationTick();
	}
}

bool GamePlayEditorBridge::Execute(const SimulationEditorCommand& command) {
	if (!IsBound() || !scene_->framework_) {
		return false;
	}
	FrameClock* frameClock = scene_->framework_->GetFrameClock();
	if (!frameClock) {
		return false;
	}

	switch (command.action) {
	case SimulationEditorAction::Play:
		frameClock->SetPaused(false);
		return true;
	case SimulationEditorAction::Pause:
		frameClock->SetPaused(true);
		return true;
	case SimulationEditorAction::Step:
		if (!frameClock->IsPaused()) {
			return false;
		}
		frameClock->RequestSingleStep();
		return true;
	default:
		return false;
	}
}

bool GamePlayEditorBridge::Execute(const GamePlayEditorCommand& command) {
	if (!IsBound() || command.farmGeneration != farmGrid_->GetGeneration()) {
		return false;
	}

	switch (command.type) {
	case GamePlayEditorCommandType::SelectFarmTile:
		return SelectCommandTarget(command);
	case GamePlayEditorCommandType::SelectFarmTileAtViewport:
		return scene_->TrySelectFarmTileFromViewport();
	case GamePlayEditorCommandType::SelectFarmTool:
		if (command.farmTool == FarmTool::BugNet) {
			return false;
		}
		scene_->farmToolSystem_.SetTool(command.farmTool);
		return true;
	case GamePlayEditorCommandType::ApplyCurrentFarmTool:
		if (!SelectCommandTarget(command)) {
			return false;
		}
		if (farmToolActionSystem_->ApplyToolDetailed(
			*farmGrid_, scene_->farmToolSystem_.GetCurrentTool(),
			scene_->farmCropSelectionSystem_.GetSelectedCrop(),
			scene_->farmEconomySystem_).Succeeded()) {
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::ApplyFarmTool:
		if (!SelectCommandTarget(command)) {
			return false;
		}
		scene_->farmToolSystem_.SetTool(command.farmTool);
		if (farmToolActionSystem_->ApplyToolDetailed(
			*farmGrid_, command.farmTool,
			scene_->farmCropSelectionSystem_.GetSelectedCrop(),
			scene_->farmEconomySystem_).Succeeded()) {
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::RaiseFarmTile:
		if (SelectCommandTarget(command) && farmToolActionSystem_->RaiseSelectedTile(*farmGrid_)) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::LowerFarmTile:
		if (SelectCommandTarget(command) && farmToolActionSystem_->LowerSelectedTile(*farmGrid_)) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::UndoFarmEdit:
		if (farmToolActionSystem_->Undo()) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::RedoFarmEdit:
		if (farmToolActionSystem_->Redo()) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::ToggleFarmCanal:
		if (SelectCommandTarget(command) &&
			farmToolActionSystem_->ToggleSelectedCanal(*farmGrid_)) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::ToggleFarmWaterSource:
		if (SelectCommandTarget(command) &&
			farmToolActionSystem_->ToggleSelectedWaterSource(*farmGrid_)) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			farmDocumentSystem_->MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::RestartFarmSession:
		scene_->ResetFarmSession();
		return true;
	default:
		return false;
	}
}

bool GamePlayEditorBridge::Execute(const FarmDocumentCommand& command) {
	if (!IsBound()) {
		return false;
	}

	bool executed = false;
	switch (command.type) {
	case FarmDocumentCommandType::NewDocument:
		executed = farmDocumentSystem_->Reset(
			*farmGrid_, scene_->farmEconomySystem_,
			scene_->farmCropSelectionSystem_);
		break;
	case FarmDocumentCommandType::Load:
		executed = farmDocumentSystem_->Load(
			command.documentId, *farmGrid_, scene_->farmEconomySystem_,
			scene_->farmCropSelectionSystem_);
		break;
	case FarmDocumentCommandType::Save:
		return farmDocumentSystem_->Save(
			*farmGrid_, scene_->farmEconomySystem_,
			scene_->farmCropSelectionSystem_);
	case FarmDocumentCommandType::SaveAs:
		return farmDocumentSystem_->SaveAs(
			command.displayName, *farmGrid_, scene_->farmEconomySystem_,
			scene_->farmCropSelectionSystem_);
	case FarmDocumentCommandType::Rename:
		return farmDocumentSystem_->Rename(command.documentId, command.displayName);
	case FarmDocumentCommandType::Delete:
		return farmDocumentSystem_->Delete(command.documentId);
	default:
		return false;
	}

	if (executed) {
		farmToolActionSystem_->ClearHistory();
		if (command.type == FarmDocumentCommandType::NewDocument ||
			command.type == FarmDocumentCommandType::Load) {
			farmIrrigationSystem_->Rebuild(*farmGrid_);
			scene_->farmProgressionSystem_.Initialize();
			static_cast<void>(scene_->farmProgressionSystem_.EvaluateClear(
				scene_->farmEconomySystem_.GetMoney()));
			scene_->farmFeedbackSystem_.Clear();
		}
	}
	return executed;
}

bool GamePlayEditorBridge::Execute(const VisibilityEditorCommand& command) {
	if (!IsBound() || !command.apply) {
		return false;
	}
	scene_->selectedTarget_ = std::clamp(command.value.selectedTarget, 0, 4);
	scene_->showTerrain_ = command.value.showTerrain;
	scene_->showSphere_ = command.value.showSphere;
	scene_->showPlane_ = command.value.showPlane;
	scene_->showSprite_ = command.value.showSprite;
	scene_->showParticles_ = command.value.showParticles;
	scene_->showAnimModel_ = command.value.showAnimatedModel;
	scene_->showSkeleton_ = command.value.showSkeleton;
	scene_->showLevelObjects_ = command.value.showLevelObjects;
	scene_->showLevelGizmos_ = command.value.showLevelGizmos;
	scene_->showLevelCollisionGizmos_ = command.value.showCollisionGizmos;
	scene_->cullMode_ = std::clamp(command.value.cullMode, 0, 2);
	return true;
}

bool GamePlayEditorBridge::Execute(const CameraEditorCommand& command) {
	if (!IsBound()) {
		return false;
	}
	if (command.reset) {
		scene_->ResetCamera();
		return true;
	}
	if (!command.apply) {
		return false;
	}
	scene_->cameraPos_ = command.value.position;
	scene_->cameraRot_ = command.value.rotation;
	scene_->cameraMoveSpeed_ = std::clamp(command.value.moveSpeed, 0.0f, 100.0f);
	scene_->cameraRotateSpeed_ = std::clamp(command.value.rotateSpeed, 0.0f, 10.0f);
	return true;
}

bool GamePlayEditorBridge::Execute(const ObjectInspectorEditorCommand& command) {
	if (!IsBound()) {
		return false;
	}
	bool executed = false;
	if (command.directionalLightChanged) {
		scene_->lightDirection_ = NormalizeOrKeep(command.directionalLight.direction, scene_->lightDirection_);
		scene_->lightColor_ = command.directionalLight.color;
		scene_->lightIntensity_ = std::clamp(command.directionalLight.intensity, 0.0f, 10.0f);
		executed = true;
	}
	if (command.spotLightChanged) {
		scene_->spotLightColor_ = command.spotLight.color;
		scene_->spotLightPos_ = command.spotLight.position;
		scene_->spotLightIntensity_ = std::clamp(command.spotLight.intensity, 0.0f, 20.0f);
		scene_->spotLightDir_ = NormalizeOrKeep(command.spotLight.direction, scene_->spotLightDir_);
		scene_->spotLightDistance_ = std::clamp(command.spotLight.distance, 1.0f, 100.0f);
		scene_->spotLightDecay_ = std::clamp(command.spotLight.decay, 0.1f, 10.0f);
		scene_->spotLightAngle_ = std::clamp(command.spotLight.angleRadians, 0.0f, 1.5707963f);
		scene_->spotLightFalloff_ = std::clamp(command.spotLight.falloffRadians, 0.0f, 1.5707963f);
		executed = true;
	}
	if (command.sphereGeometryChanged) {
		scene_->sphereRadius_ = std::clamp(command.sphere.radius, 0.1f, 10.0f);
		scene_->CreateSphere(scene_->sphereRadius_);
		executed = true;
	}
	if (command.sphereTransformChanged) {
		scene_->spherePos_ = command.sphere.position;
		scene_->objectRot_ = command.sphere.rotation;
		executed = true;
	}
	if (command.animationModelChanged) {
		const int modelIndex = std::clamp(command.animation.modelIndex, 0, 3);
		scene_->currentAnimModelIdx_ = modelIndex;
		scene_->ChangeAnimationModel(modelIndex);
		executed = true;
	}
	if (command.animationVisibilityChanged) {
		scene_->showAnimModel_ = command.animation.showModel;
		executed = true;
	}
	if (command.animationPlaybackChanged && scene_->animObj_) {
		scene_->animObj_->GetIsAnimationPlaying() = command.animation.playing;
		scene_->animObj_->GetAnimationSpeed() = std::clamp(command.animation.speed, -5.0f, 5.0f);
		const float duration = scene_->animObj_->GetAnimation().duration;
		scene_->animObj_->GetAnimationTime() = duration > 0.0f
			? std::clamp(command.animation.time, 0.0f, duration)
			: 0.0f;
		executed = true;
	}
	if (command.cylinderChanged) {
		scene_->cylTopRadius_ = std::clamp(command.cylinder.topRadius, 0.0f, 5.0f);
		scene_->cylBottomRadius_ = std::clamp(command.cylinder.bottomRadius, 0.0f, 5.0f);
		scene_->cylHeight_ = std::clamp(command.cylinder.height, 0.1f, 10.0f);
		scene_->cylSegments_ = std::clamp(command.cylinder.segments, 3, 64);
		scene_->cylVertDivisions_ = std::clamp(command.cylinder.verticalDivisions, 1, 16);
		scene_->RebuildCylinder();
		executed = true;
	}
	if (command.jointChanged && scene_->animObj_) {
		std::optional<Skeleton>& skeleton = scene_->animObj_->GetSkeleton();
		if (skeleton.has_value() && command.joint.jointIndex >= 0 &&
			command.joint.jointIndex < static_cast<int>(skeleton->joints.size())) {
			Joint& joint = skeleton->joints[static_cast<std::size_t>(command.joint.jointIndex)];
			joint.transform.translate = command.joint.translate;
			joint.transform.scale = {
				(std::max)(command.joint.scale.x, 0.01f),
				(std::max)(command.joint.scale.y, 0.01f),
				(std::max)(command.joint.scale.z, 0.01f),
			};
			executed = true;
		}
	}
	return executed;
}

bool GamePlayEditorBridge::Execute(const ParticleEditorCommand& command) {
	if (!IsBound()) {
		return false;
	}
	ParticleManager* particleManager = scene_->framework_ ? scene_->framework_->GetParticleManager() : nullptr;
	bool executed = false;
	if (command.activeParticleTypeChanged) {
		scene_->activeParticleType_ = std::clamp(command.activeParticleType, 0, 4);
		executed = true;
	}
	if (command.gpuModeChanged) {
		scene_->SetGPUParticleDebugMode(ToSceneGpuParticleMode(command.gpuMode));
		executed = true;
	}
	if (command.agricultureSettingsChanged) {
		scene_->agricultureEmitPosition_ = command.agriculture.emitPosition;
		scene_->agricultureParticleSize_ = std::clamp(command.agriculture.particleSize, 0.02f, 1.0f);
		scene_->agricultureParticleCount_ = std::clamp(command.agriculture.particleCount, 1, 1024);
		scene_->agricultureShowKeyGuide_ = command.agriculture.showKeyGuide;
		executed = true;
	}
	if (command.emitAgricultureParticle) {
		scene_->EmitAgricultureParticle(ToSceneAgricultureParticleType(command.agricultureParticleType));
		executed = true;
	}
	if (command.interactionSettingsChanged) {
		const int previousGridCount = scene_->interactionGridCount_;
		const float previousParticleSize = scene_->interactionParticleSize_;
		scene_->interactionGridCount_ = std::clamp(command.interaction.gridCount, 2, 10);
		scene_->interactionParticleSize_ = std::clamp(command.interaction.particleSize, 0.02f, 0.05f);
		scene_->interactionBrushRadius_ = std::clamp(command.interaction.brushRadius, 0.1f, 10.0f);
		scene_->interactionBrushStrength_ = std::clamp(command.interaction.brushStrength, 0.0f, 10.0f);
		scene_->interactionDamping_ = std::clamp(command.interaction.damping, 0.80f, 0.995f);
		scene_->interactionParticleCount_ = scene_->CalculateInteractionParticleCount();
		if (scene_->interactionGridCount_ != previousGridCount ||
			scene_->interactionParticleSize_ != previousParticleSize) {
			scene_->interactionResetRequested_ = true;
		}
		executed = true;
	}
	if (command.resetInteractionGrid && particleManager) {
		particleManager->ResetGPUParticles();
		scene_->interactionResetRequested_ = true;
		executed = true;
	}
	if (command.alphaReferenceChanged && particleManager) {
		particleManager->SetAlphaReference(std::clamp(command.alphaReference, 0.0f, 1.0f));
		executed = true;
	}
	if (command.clearAll && particleManager) {
		particleManager->ClearAll();
		executed = true;
	}
	return executed;
}

void GamePlayEditorBridge::SetViewportState(
	const Vector2& imageTopLeft,
	const Vector2& imageSize,
	const Vector2& mousePosition,
	bool hovered,
	bool focused) noexcept {
	if (!scene_) {
		return;
	}
	scene_->viewportImageTopLeft_ = imageTopLeft;
	scene_->viewportImageSize_ = imageSize;
	scene_->viewportMousePosition_ = mousePosition;
	scene_->viewportHovered_ = hovered;
	scene_->viewportFocused_ = focused;
}

Object3d* GamePlayEditorBridge::GetAnimationObjectForDebug() const noexcept {
	return scene_ ? scene_->animObj_.get() : nullptr;
}

bool GamePlayEditorBridge::SelectCommandTarget(const GamePlayEditorCommand& command) {
	return farmGrid_ != nullptr && farmGrid_->SetSelectedIndex(command.farmTileIndex);
}

} // namespace editor
