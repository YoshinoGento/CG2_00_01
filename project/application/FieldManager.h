#pragma once

#include "math/Matrix.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class Camera;
class Framework;
class Model;
class Object3d;

enum class FieldState {
	Empty,
	Tilled,
	Watered,
	Planted,
	ReadyToHarvest,
};

enum class FieldActionFeedbackType {
	None,
	Tilled,
	Watered,
	Planted,
	Harvested,
};

struct FieldTile {
	Vector3 worldPosition = { 0.0f, 0.0f, 0.0f };
	FieldState state = FieldState::Empty;
	int cropType = 0;
	float growth = 0.0f;
	float moisture = 0.0f;
	float actionFlashTimer = 0.0f;
	Vector4 actionFlashColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	std::unique_ptr<Object3d> groundObject;
	std::unique_ptr<Object3d> moundObject;
	std::unique_ptr<Object3d> cropStemObject;
	std::unique_ptr<Object3d> cropFruitObject;
	std::unique_ptr<Object3d> cropLeafLeftObject;
	std::unique_ptr<Object3d> cropLeafRightObject;
};

class FieldManager {
public:
	~FieldManager();

	void Initialize(Framework* framework);
	void Update(float deltaTime, Camera* camera);
	void Draw();
	void DrawImGui();

	void TillTile(int index);
	void WaterTile(int index);
	void PlantTile(int index);
	void HarvestTile(int index);

	FieldTile* GetSelectedTile();
	const FieldTile* GetSelectedTile() const;
	const FieldTile* GetTile(int index) const;
	int GetSelectedIndex() const { return selectedIndex_; }
	int GetTileCount() const { return static_cast<int>(tiles_.size()); }
	int GetVisibleTileCount() const;
	Vector3 GetFieldCenter() const;
	Vector3 GetDemoFieldWorldPosition(int index) const;
	float GetGroundY() const;
	void SelectTile(int index);
	void ResetAllTilesForAutoDemo(int selectedIndex);
	void PrepareTileForAutoDemo(int index);
	void FastForwardSelectedGrowth(float amount);
	bool FastForwardAllGrowth(float amount);
	void SetInputEnabled(bool enabled) { inputEnabled_ = enabled; }
	bool TrySelectTileByWorldPosition(const Vector3& worldPosition);

	bool ConsumeHarvestEvent(Vector3& outPosition, int32_t& outPrice, bool& outRare);
	bool ConsumeActionFeedbackEvent(FieldActionFeedbackType& outType, Vector3& outPosition);

private:
	struct FakePebble {
		bool active = false;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		float timer = 0.0f;
	float duration = 0.7f;
	std::unique_ptr<Object3d> object;
	};

	static constexpr int kGridWidth = 3;
	static constexpr int kGridHeight = 3;
	static constexpr int kMaxPebbles = 24;

	void HandleInput();
	void UpdateGrowth(float deltaTime);
	void UpdateTileVisuals(Camera* camera);
	void UpdatePebbles(float deltaTime, Camera* camera);
	void MoveSelection(int dx, int dz);
	int FindTileIndexFromWorldPosition(const Vector3& worldPosition) const;
	void EmitDirtFeedback(const Vector3& position);
	void EmitWaterFeedback(const Vector3& position);
	void EmitPlantFeedback(const Vector3& position);
	void SpawnPebbles(const Vector3& position);
	void SetObjectColor(Object3d* object, const Vector4& color);
	void InitializeCropPart(std::unique_ptr<Object3d>& object, Model* model);
	void HideAndUpdateObject(Object3d* object, Camera* camera);
	void SetTileFlash(FieldTile& tile, const Vector4& color, float duration);
	void SetActionFeedback(FieldActionFeedbackType type, const Vector3& position);
	Vector4 GetGroundColor(const FieldTile& tile, bool selected) const;
	bool IsValidIndex(int index) const;

	Framework* framework_ = nullptr;
	std::unique_ptr<Model> tileModel_;
	std::unique_ptr<Model> moundModel_;
	std::unique_ptr<Model> cropPartModel_;
	std::unique_ptr<Model> pebbleModel_;
	uint32_t fieldTextureHandle_ = 0;
	std::vector<FieldTile> tiles_;
	std::vector<FakePebble> pebbles_;
	int selectedIndex_ = 0;
	int32_t harvestCount_ = 0;
	float selectionPulseTimer_ = 0.0f;
	bool fieldVisualCullNoneTest_ = false;
	bool inputEnabled_ = true;

	bool pendingHarvestEvent_ = false;
	Vector3 pendingHarvestPosition_ = { 0.0f, 0.0f, 0.0f };
	int32_t pendingHarvestPrice_ = 120;
	bool pendingHarvestRare_ = false;
	bool pendingActionFeedback_ = false;
	FieldActionFeedbackType pendingActionFeedbackType_ = FieldActionFeedbackType::None;
	Vector3 pendingActionFeedbackPosition_ = { 0.0f, 0.0f, 0.0f };

	std::mt19937 randomEngine_{ 0x1234abcd };
};
