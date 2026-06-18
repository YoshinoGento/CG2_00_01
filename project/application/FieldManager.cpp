#include "FieldManager.h"

#include "3d/Camera.h"
#include "3d/LineDrawer.h"
#include "3d/Model.h"
#include "3d/Object3d.h"
#include "3d/PrimitiveGenerator.h"
#include "2d/SpriteCommon.h"
#include "base/Framework.h"
#include "base/ImGuiManager.h"
#include "effect/ParticleManager.h"
#include "io/Input.h"

#include <algorithm>
#include <cmath>
#include <dinput.h>
#include <numbers>

namespace {
constexpr float kGroundY = -1.92f;
constexpr float kTileSpacing = 2.15f;
constexpr float kCropBaseHeight = 0.9f;
constexpr float kGrowthPerSecondDry = 0.22f;
constexpr float kGrowthPerSecondWet = 0.48f;
constexpr float kMoistureDecayPerSecond = 0.08f;
constexpr float kPebbleGravity = 5.0f;
constexpr uint32_t kDefaultTextureHandle = 0;

Vector4 LerpColor(const Vector4& a, const Vector4& b, float t) {
	const float clampedT = std::clamp(t, 0.0f, 1.0f);
	return {
		a.x + (b.x - a.x) * clampedT,
		a.y + (b.y - a.y) * clampedT,
		a.z + (b.z - a.z) * clampedT,
		a.w + (b.w - a.w) * clampedT,
	};
}
}

FieldManager::~FieldManager() = default;

void FieldManager::Initialize(Framework* framework) {
	framework_ = framework;
	if (!framework_) {
		return;
	}

	tileModel_ = PrimitiveGenerator::CreatePlane(framework_->GetModelManager(), 1.85f, 1.85f);
	furrowModel_ = PrimitiveGenerator::CreateBox(framework_->GetModelManager(), { 1.55f, 0.045f, 0.085f });
	waterHighlightModel_ = PrimitiveGenerator::CreateBox(framework_->GetModelManager(), { 0.28f, 0.018f, 0.16f });
	cropModel_ = PrimitiveGenerator::CreateBox(framework_->GetModelManager(), { 0.45f, 1.0f, 0.45f });
	pebbleModel_ = PrimitiveGenerator::CreateSphere(framework_->GetModelManager(), 0.08f, 8);
	fieldTextureHandle_ = framework_->GetSpriteCommon()
		? framework_->GetSpriteCommon()->LoadTexture("Resources/human/white.png")
		: kDefaultTextureHandle;

	tiles_.clear();
	tiles_.resize(kGridWidth * kGridHeight);
	const Vector3 gridCenter = { 0.0f, kGroundY, 8.0f };
	for (int z = 0; z < kGridHeight; ++z) {
		for (int x = 0; x < kGridWidth; ++x) {
			const int index = z * kGridWidth + x;
			FieldTile& tile = tiles_[index];
			tile.worldPosition = {
				gridCenter.x + (static_cast<float>(x) - 1.0f) * kTileSpacing,
				gridCenter.y,
				gridCenter.z + (static_cast<float>(z) - 1.0f) * kTileSpacing,
			};

			tile.groundObject = std::make_unique<Object3d>();
			tile.groundObject->Initialize(framework_->GetObject3dCommon());
			tile.groundObject->SetModel(tileModel_.get());
			tile.groundObject->SetTexture(fieldTextureHandle_);
			tile.groundObject->SetPosition(tile.worldPosition);
			tile.groundObject->SetEnvironmentCoefficient(0.0f);

			for (auto& furrowObject : tile.furrowObjects) {
				furrowObject = std::make_unique<Object3d>();
				furrowObject->Initialize(framework_->GetObject3dCommon());
				furrowObject->SetModel(furrowModel_.get());
				furrowObject->SetTexture(fieldTextureHandle_);
				furrowObject->SetScale({ 0.0f, 0.0f, 0.0f });
				furrowObject->SetEnvironmentCoefficient(0.0f);
			}

			for (auto& waterHighlightObject : tile.waterHighlightObjects) {
				waterHighlightObject = std::make_unique<Object3d>();
				waterHighlightObject->Initialize(framework_->GetObject3dCommon());
				waterHighlightObject->SetModel(waterHighlightModel_.get());
				waterHighlightObject->SetTexture(fieldTextureHandle_);
				waterHighlightObject->SetScale({ 0.0f, 0.0f, 0.0f });
				waterHighlightObject->SetEnvironmentCoefficient(0.0f);
			}

			tile.cropObject = std::make_unique<Object3d>();
			tile.cropObject->Initialize(framework_->GetObject3dCommon());
			tile.cropObject->SetModel(cropModel_.get());
			tile.cropObject->SetTexture(fieldTextureHandle_);
			tile.cropObject->SetPosition(tile.worldPosition + Vector3{ 0.0f, 0.25f, 0.0f });
			tile.cropObject->SetEnvironmentCoefficient(0.0f);
			tile.cropObject->SetScale({ 0.0f, 0.0f, 0.0f });
		}
	}

	pebbles_.resize(kMaxPebbles);
	for (FakePebble& pebble : pebbles_) {
		pebble.object = std::make_unique<Object3d>();
		pebble.object->Initialize(framework_->GetObject3dCommon());
		pebble.object->SetModel(pebbleModel_.get());
		pebble.object->SetTexture(fieldTextureHandle_);
		pebble.object->SetScale({ 0.0f, 0.0f, 0.0f });
		SetObjectColor(pebble.object.get(), { 0.38f, 0.22f, 0.11f, 1.0f });
	}
}

void FieldManager::Update(float deltaTime, Camera* camera) {
	if (!framework_ || !camera) {
		return;
	}

	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.25f);
	selectionPulseTimer_ += safeDeltaTime;
	HandleInput();
	UpdateGrowth(safeDeltaTime);
	UpdateTileVisuals(camera);
	UpdatePebbles(safeDeltaTime, camera);
}

void FieldManager::Draw() {
	for (const FieldTile& tile : tiles_) {
		if (tile.groundObject) {
			tile.groundObject->Draw();
		}
		if (tile.state != FieldState::Empty) {
			for (const auto& furrowObject : tile.furrowObjects) {
				if (furrowObject) {
					furrowObject->Draw();
				}
			}
		}
		if (tile.state == FieldState::Watered) {
			for (const auto& waterHighlightObject : tile.waterHighlightObjects) {
				if (waterHighlightObject) {
					waterHighlightObject->Draw();
				}
			}
		}
		if ((tile.state == FieldState::Planted || tile.state == FieldState::ReadyToHarvest) && tile.cropObject) {
			tile.cropObject->Draw();
		}
	}

	for (const FakePebble& pebble : pebbles_) {
		if (pebble.active && pebble.object) {
			pebble.object->Draw();
		}
	}

	const FieldTile* selectedTile = GetSelectedTile();
	if (selectedTile) {
		const float pulse = 0.5f + 0.5f * std::sin(selectionPulseTimer_ * 7.5f);
		const Vector3 center = selectedTile->worldPosition + Vector3{ 0.0f, 0.14f + 0.035f * pulse, 0.0f };
		const float half = 1.04f + 0.05f * pulse;
		const Vector4 color = { 1.0f, 0.95f, 0.05f + 0.15f * pulse, 1.0f };
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ -half, 0.0f, -half }, center + Vector3{ half, 0.0f, -half }, color);
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ half, 0.0f, -half }, center + Vector3{ half, 0.0f, half }, color);
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ half, 0.0f, half }, center + Vector3{ -half, 0.0f, half }, color);
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ -half, 0.0f, half }, center + Vector3{ -half, 0.0f, -half }, color);
		const float innerHalf = half * 0.88f;
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ -innerHalf, 0.02f, -innerHalf }, center + Vector3{ innerHalf, 0.02f, -innerHalf }, color);
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ innerHalf, 0.02f, -innerHalf }, center + Vector3{ innerHalf, 0.02f, innerHalf }, color);
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ innerHalf, 0.02f, innerHalf }, center + Vector3{ -innerHalf, 0.02f, innerHalf }, color);
		LineDrawer::GetInstance()->DrawLine(center + Vector3{ -innerHalf, 0.02f, innerHalf }, center + Vector3{ -innerHalf, 0.02f, -innerHalf }, color);
	}
}

void FieldManager::DrawImGui() {
	const FieldTile* selectedTile = GetSelectedTile();
	const char* stateName = "None";
	if (selectedTile) {
		switch (selectedTile->state) {
		case FieldState::Empty:
			stateName = "Empty";
			break;
		case FieldState::Tilled:
			stateName = "Tilled";
			break;
		case FieldState::Watered:
			stateName = "Watered";
			break;
		case FieldState::Planted:
			stateName = "Planted";
			break;
		case FieldState::ReadyToHarvest:
			stateName = "ReadyToHarvest";
			break;
		}
	}

	if (ImGui::CollapsingHeader("Field Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Selected Tile: %d / %d", selectedIndex_, static_cast<int>(tiles_.size()));
		ImGui::Text("State: %s", stateName);
		if (selectedTile) {
			ImGui::Text("Position: %.2f, %.2f, %.2f",
				selectedTile->worldPosition.x,
				selectedTile->worldPosition.y,
				selectedTile->worldPosition.z);
			ImGui::Text("Growth: %.2f", selectedTile->growth);
			ImGui::Text("Moisture: %.2f", selectedTile->moisture);
		}
		ImGui::TextUnformatted("Left Click on Game Viewport: Select tile");
		ImGui::TextUnformatted("T: Till  Y: Water  P: Plant  H: Harvest");
		if (ImGui::Button("Till")) {
			TillTile(selectedIndex_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Water")) {
			WaterTile(selectedIndex_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Plant")) {
			PlantTile(selectedIndex_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Harvest")) {
			HarvestTile(selectedIndex_);
		}
		ImGui::Text("Harvest Count: %d", harvestCount_);
		ImGui::TextUnformatted("Every third harvest is treated as Rare and also triggers Digital Impact.");
	}
}

void FieldManager::TillTile(int index) {
	if (!IsValidIndex(index)) {
		return;
	}
	FieldTile& tile = tiles_[index];
	if (tile.state != FieldState::Empty) {
		return;
	}

	tile.state = FieldState::Tilled;
	tile.moisture = 0.0f;
	tile.growth = 0.0f;
	SetTileFlash(tile, { 0.95f, 0.62f, 0.28f, 1.0f }, 0.25f);
	SetActionFeedback(FieldActionFeedbackType::Tilled, tile.worldPosition + Vector3{ 0.0f, 0.55f, 0.0f });
	EmitDirtFeedback(tile.worldPosition);
	SpawnPebbles(tile.worldPosition);
}

void FieldManager::WaterTile(int index) {
	if (!IsValidIndex(index)) {
		return;
	}
	FieldTile& tile = tiles_[index];
	if (tile.state != FieldState::Tilled && tile.state != FieldState::Planted) {
		return;
	}

	tile.moisture = 1.0f;
	if (tile.state == FieldState::Tilled) {
		tile.state = FieldState::Watered;
	}
	SetTileFlash(tile, { 0.35f, 0.75f, 1.0f, 1.0f }, 0.25f);
	SetActionFeedback(FieldActionFeedbackType::Watered, tile.worldPosition + Vector3{ 0.0f, 0.55f, 0.0f });
	EmitWaterFeedback(tile.worldPosition);
}

void FieldManager::PlantTile(int index) {
	if (!IsValidIndex(index)) {
		return;
	}
	FieldTile& tile = tiles_[index];
	if (tile.state != FieldState::Tilled && tile.state != FieldState::Watered) {
		return;
	}

	tile.state = FieldState::Planted;
	tile.cropType = 0;
	tile.growth = 0.05f;
	SetTileFlash(tile, { 0.35f, 1.0f, 0.35f, 1.0f }, 0.25f);
	SetActionFeedback(FieldActionFeedbackType::Planted, tile.worldPosition + Vector3{ 0.0f, 0.70f, 0.0f });
	EmitPlantFeedback(tile.worldPosition);
}

void FieldManager::HarvestTile(int index) {
	if (!IsValidIndex(index)) {
		return;
	}
	FieldTile& tile = tiles_[index];
	if (tile.state != FieldState::ReadyToHarvest) {
		return;
	}

	++harvestCount_;
	const bool rareHarvest = (harvestCount_ % 3) == 0;
	pendingHarvestEvent_ = true;
	pendingHarvestPosition_ = tile.worldPosition + Vector3{ 0.0f, 0.5f, 0.0f };
	pendingHarvestPrice_ = rareHarvest ? 500 : 120;
	pendingHarvestRare_ = rareHarvest;

	tile.state = FieldState::Tilled;
	tile.cropType = 0;
	tile.growth = 0.0f;
	tile.moisture = 0.35f;
	SetTileFlash(tile, rareHarvest ? Vector4{ 0.25f, 0.85f, 1.0f, 1.0f } : Vector4{ 1.0f, 0.85f, 0.20f, 1.0f }, 0.32f);
	SetActionFeedback(FieldActionFeedbackType::Harvested, pendingHarvestPosition_);
}

FieldTile* FieldManager::GetSelectedTile() {
	if (!IsValidIndex(selectedIndex_)) {
		return nullptr;
	}
	return &tiles_[selectedIndex_];
}

const FieldTile* FieldManager::GetSelectedTile() const {
	if (!IsValidIndex(selectedIndex_)) {
		return nullptr;
	}
	return &tiles_[selectedIndex_];
}

bool FieldManager::ConsumeHarvestEvent(Vector3& outPosition, int32_t& outPrice, bool& outRare) {
	if (!pendingHarvestEvent_) {
		return false;
	}
	pendingHarvestEvent_ = false;
	outPosition = pendingHarvestPosition_;
	outPrice = pendingHarvestPrice_;
	outRare = pendingHarvestRare_;
	return true;
}

bool FieldManager::ConsumeActionFeedbackEvent(FieldActionFeedbackType& outType, Vector3& outPosition) {
	if (!pendingActionFeedback_) {
		return false;
	}
	pendingActionFeedback_ = false;
	outType = pendingActionFeedbackType_;
	outPosition = pendingActionFeedbackPosition_;
	return true;
}

float FieldManager::GetGroundY() const {
	return kGroundY;
}

bool FieldManager::TrySelectTileByWorldPosition(const Vector3& worldPosition) {
	const int index = FindTileIndexFromWorldPosition(worldPosition);
	if (!IsValidIndex(index)) {
		return false;
	}
	selectedIndex_ = index;
	return true;
}

void FieldManager::HandleInput() {
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (!input || ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}

	if (input->TriggerKey(DIK_T)) {
		TillTile(selectedIndex_);
	}
	if (input->TriggerKey(DIK_Y)) {
		WaterTile(selectedIndex_);
	}
	if (input->TriggerKey(DIK_P)) {
		PlantTile(selectedIndex_);
	}
	if (input->TriggerKey(DIK_H)) {
		HarvestTile(selectedIndex_);
	}
}

void FieldManager::UpdateGrowth(float deltaTime) {
	for (FieldTile& tile : tiles_) {
		tile.moisture = (std::max)(tile.moisture - kMoistureDecayPerSecond * deltaTime, 0.0f);
		tile.actionFlashTimer = (std::max)(tile.actionFlashTimer - deltaTime, 0.0f);
		if (tile.state != FieldState::Planted) {
			continue;
		}

		const float growthSpeed = tile.moisture > 0.05f ? kGrowthPerSecondWet : kGrowthPerSecondDry;
		tile.growth = std::clamp(tile.growth + growthSpeed * deltaTime, 0.0f, 1.0f);
		if (tile.growth >= 1.0f) {
			tile.state = FieldState::ReadyToHarvest;
			tile.moisture = (std::max)(tile.moisture, 0.2f);
		}
	}
}

void FieldManager::UpdateTileVisuals(Camera* camera) {
	for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
		FieldTile& tile = tiles_[i];
		const bool selected = i == selectedIndex_;
		if (tile.groundObject) {
			tile.groundObject->SetPosition(tile.worldPosition);
			SetObjectColor(tile.groundObject.get(), GetGroundColor(tile, selected));
			tile.groundObject->Update(camera);
		}

		if (tile.cropObject) {
			const bool cropVisible = tile.state == FieldState::Planted || tile.state == FieldState::ReadyToHarvest;
			const float growth = tile.state == FieldState::ReadyToHarvest ? 1.0f : std::clamp(tile.growth, 0.0f, 1.0f);
			if (cropVisible) {
				const float scale = 0.35f + 0.75f * growth;
				tile.cropObject->SetScale({ scale, 0.25f + kCropBaseHeight * growth, scale });
				tile.cropObject->SetPosition(tile.worldPosition + Vector3{ 0.0f, 0.12f + 0.45f * growth, 0.0f });
				SetObjectColor(tile.cropObject.get(), GetCropColor(tile));
			} else {
				tile.cropObject->SetScale({ 0.0f, 0.0f, 0.0f });
			}
			tile.cropObject->Update(camera);
		}
	}
}

void FieldManager::UpdatePebbles(float deltaTime, Camera* camera) {
	for (FakePebble& pebble : pebbles_) {
		if (!pebble.active) {
			continue;
		}

		pebble.timer += deltaTime;
		pebble.velocity.y -= kPebbleGravity * deltaTime;
		pebble.position += pebble.velocity * deltaTime;
		if (pebble.position.y < kGroundY + 0.07f) {
			pebble.position.y = kGroundY + 0.07f;
			pebble.velocity.y *= -0.35f;
			pebble.velocity.x *= 0.70f;
			pebble.velocity.z *= 0.70f;
		}
		if (pebble.timer >= pebble.duration) {
			pebble.active = false;
			if (pebble.object) {
				pebble.object->SetScale({ 0.0f, 0.0f, 0.0f });
				pebble.object->Update(camera);
			}
			continue;
		}

		if (pebble.object) {
			const float normalized = std::clamp(pebble.timer / pebble.duration, 0.0f, 1.0f);
			const float scale = 1.0f - normalized * 0.45f;
			pebble.object->SetPosition(pebble.position);
			pebble.object->SetScale({ scale, scale, scale });
			pebble.object->Update(camera);
		}
	}
}

void FieldManager::MoveSelection(int dx, int dz) {
	const int x = selectedIndex_ % kGridWidth;
	const int z = selectedIndex_ / kGridWidth;
	const int nextX = std::clamp(x + dx, 0, kGridWidth - 1);
	const int nextZ = std::clamp(z + dz, 0, kGridHeight - 1);
	selectedIndex_ = nextZ * kGridWidth + nextX;
}

int FieldManager::FindTileIndexFromWorldPosition(const Vector3& worldPosition) const {
	int bestIndex = -1;
	constexpr float kSelectionHalfExtent = 1.05f;
	float bestDistSq = kSelectionHalfExtent * kSelectionHalfExtent * 2.0f;

	for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
		const FieldTile& tile = tiles_[i];
		const float dx = worldPosition.x - tile.worldPosition.x;
		const float dz = worldPosition.z - tile.worldPosition.z;
		if (std::abs(dx) > kSelectionHalfExtent || std::abs(dz) > kSelectionHalfExtent) {
			continue;
		}

		const float distSq = dx * dx + dz * dz;
		if (distSq < bestDistSq) {
			bestDistSq = distSq;
			bestIndex = i;
		}
	}

	return bestIndex;
}

void FieldManager::EmitDirtFeedback(const Vector3& position) {
	if (!framework_ || !framework_->GetParticleManager()) {
		return;
	}
	GPUParticleEmitSettings settings{};
	settings.translate = position + Vector3{ 0.0f, 0.15f, 0.0f };
	settings.radius = 0.55f;
	settings.color = { 0.45f, 0.27f, 0.12f, 1.0f };
	settings.scale = { 0.12f, 0.12f, 0.12f };
	settings.lifeTime = 0.75f;
	settings.baseVelocity = { 0.0f, 0.35f, 0.0f };
	settings.speed = 0.35f;
	settings.count = 64;
	settings.emit = 1;
	settings.preset = 0;
	framework_->GetParticleManager()->RequestGPUParticleEmit(settings);
}

void FieldManager::EmitWaterFeedback(const Vector3& position) {
	if (!framework_ || !framework_->GetParticleManager()) {
		return;
	}
	GPUParticleEmitSettings settings{};
	settings.translate = position + Vector3{ 0.0f, 0.25f, 0.0f };
	settings.radius = 0.45f;
	settings.color = { 0.45f, 0.85f, 1.0f, 1.0f };
	settings.scale = { 0.09f, 0.09f, 0.09f };
	settings.lifeTime = 0.65f;
	settings.baseVelocity = { 0.0f, 0.18f, 0.0f };
	settings.speed = 1.0f;
	settings.count = 48;
	settings.emit = 1;
	settings.preset = 1;
	framework_->GetParticleManager()->RequestGPUParticleEmit(settings);
}

void FieldManager::EmitPlantFeedback(const Vector3& position) {
	if (!framework_ || !framework_->GetParticleManager()) {
		return;
	}
	GPUParticleEmitSettings settings{};
	settings.translate = position + Vector3{ 0.0f, 0.35f, 0.0f };
	settings.radius = 0.35f;
	settings.color = { 0.55f, 0.95f, 0.25f, 1.0f };
	settings.scale = { 0.08f, 0.08f, 0.08f };
	settings.lifeTime = 0.9f;
	settings.baseVelocity = { 0.0f, 0.55f, 0.0f };
	settings.speed = 0.25f;
	settings.count = 36;
	settings.emit = 1;
	settings.preset = 3;
	framework_->GetParticleManager()->RequestGPUParticleEmit(settings);
}

void FieldManager::SpawnPebbles(const Vector3& position) {
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> speedDist(0.45f, 1.25f);
	std::uniform_real_distribution<float> lifeDist(0.45f, 0.85f);

	int spawned = 0;
	for (FakePebble& pebble : pebbles_) {
		if (pebble.active) {
			continue;
		}
		const float angle = angleDist(randomEngine_);
		const float speed = speedDist(randomEngine_);
		pebble.active = true;
		pebble.position = position + Vector3{ 0.0f, 0.12f, 0.0f };
		pebble.velocity = {
			std::cos(angle) * speed,
			1.2f + speed * 0.35f,
			std::sin(angle) * speed,
		};
		pebble.timer = 0.0f;
		pebble.duration = lifeDist(randomEngine_);
		++spawned;
		if (spawned >= 6) {
			break;
		}
	}
}

void FieldManager::SetObjectColor(Object3d* object, const Vector4& color) {
	if (!object || !object->GetMaterialData()) {
		return;
	}
	object->GetMaterialData()->color = color;
	// Field colors are state indicators. Use an unlit material path so they stay readable
	// even when the scene light direction or skybox makes ordinary 3D objects dark.
	object->GetMaterialData()->enableLighting = 0;
	object->GetMaterialData()->environmentCoefficient = 0.0f;
}

void FieldManager::SetTileFlash(FieldTile& tile, const Vector4& color, float duration) {
	tile.actionFlashColor = color;
	tile.actionFlashTimer = (std::max)(duration, 0.0f);
}

void FieldManager::SetActionFeedback(FieldActionFeedbackType type, const Vector3& position) {
	pendingActionFeedback_ = type != FieldActionFeedbackType::None;
	pendingActionFeedbackType_ = type;
	pendingActionFeedbackPosition_ = position;
}

Vector4 FieldManager::GetGroundColor(const FieldTile& tile, bool selected) const {
	Vector4 color = { 0.45f, 0.75f, 0.28f, 1.0f };
	switch (tile.state) {
	case FieldState::Empty:
		color = { 0.45f, 0.75f, 0.28f, 1.0f };
		break;
	case FieldState::Tilled:
		color = { 0.72f, 0.42f, 0.20f, 1.0f };
		break;
	case FieldState::Watered:
		color = { 0.32f, 0.48f, 0.62f, 1.0f };
		break;
	case FieldState::Planted:
		color = tile.moisture > 0.05f
			? Vector4{ 0.62f, 0.36f, 0.18f, 1.0f }
			: Vector4{ 0.70f, 0.40f, 0.20f, 1.0f };
		break;
	case FieldState::ReadyToHarvest:
		color = { 0.70f, 0.45f, 0.20f, 1.0f };
		break;
	}

	if (tile.actionFlashTimer > 0.0f) {
		const float flashAmount = std::clamp(tile.actionFlashTimer / 0.32f, 0.0f, 1.0f);
		color = LerpColor(color, tile.actionFlashColor, flashAmount * 0.55f);
	}

	if (selected) {
		color = LerpColor(color, { 1.0f, 0.95f, 0.18f, 1.0f }, 0.22f);
	}
	return color;
}

Vector4 FieldManager::GetCropColor(const FieldTile& tile) const {
	if (tile.state == FieldState::ReadyToHarvest) {
		const float pulse = 0.5f + 0.5f * std::sin(selectionPulseTimer_ * 8.0f);
		return {
			1.0f,
			0.85f + 0.12f * pulse,
			0.15f + 0.18f * pulse,
			1.0f
		};
	}
	const float growth = std::clamp(tile.growth, 0.0f, 1.0f);
	return {
		0.20f + growth * 0.20f,
		0.95f - growth * 0.08f,
		0.25f,
		1.0f,
	};
}

bool FieldManager::IsValidIndex(int index) const {
	return index >= 0 && index < static_cast<int>(tiles_.size());
}
