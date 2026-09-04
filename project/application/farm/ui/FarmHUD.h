#pragma once

#include "2d/BitmapFont.h"
#include "2d/Sprite.h"
#include "2d/SpriteText.h"
#include "farm/core/FarmTypes.h"

#include <array>
#include <cstdint>

class SpriteCommon;

enum class FarmHUDNextAction : std::uint8_t {
	SelectTile,
	Canal,
	WaterSource,
	Hoe,
	WaterOrSeed,
	BuySeed,
	Seed,
	Water,
	Growing,
	Harvest,
};

enum class FarmHUDFeedback : std::uint8_t {
	None,
	Harvest,
	Sale,
	EmptySale,
	InputLocked,
	Restarted,
	SeedPurchasedTurnip,
	SeedPurchasedCarrot,
	CropSelectedTurnip,
	CropSelectedCarrot,
	NoSeedTurnip,
	NoSeedCarrot,
	InsufficientMoney,
};

enum class FarmHUDMoistureStatus : std::uint8_t {
	None,
	Dry,
	Low,
	Good,
};

struct FarmHUDViewData {
	int day = 1;
	int money = 0;
	int rank = 1;
	int cropCount = 0;
	int saleValue = 0;
	int seedCount = 0;
	int seedPrice = 0;
	std::array<int, farm::kFarmCropTypeCount> cropInventoryCounts{};
	std::array<int, farm::kFarmCropTypeCount> cropInventoryValues{};
	std::array<int, farm::kFarmCropTypeCount> cropSeedCounts{};
	farm::CropType selectedSeedCrop = farm::CropType::TestCrop;
	int cropsNeeded = 0;
	int goalMoney = 1;
	int currentToolIndex = -1;
	float goalProgress = 0.0f;
	float timeScale = 1.0f;
	bool goalCleared = false;
	bool irrigationPreviewActive = false;
	bool selectedTileValid = false;
	int selectedTileIndex = -1;
	int selectedTileHeight = 0;
	int selectedTileMoisturePercent = 0;
	int selectedTileGrowthPercent = 0;
	farm::FarmTileFeature selectedTileFeature = farm::FarmTileFeature::None;
	bool selectedTileIrrigationSupplied = false;
	bool selectedTileIrrigationActive = false;
	int selectedTileIrrigationStrengthPercent = 0;
	farm::FarmTileState selectedTileState = farm::FarmTileState::Empty;
	farm::CropType selectedTileCrop = farm::CropType::None;
	farm::FarmCropGrowthStage selectedTileGrowthStage = farm::FarmCropGrowthStage::None;
	FarmHUDMoistureStatus selectedTileMoistureStatus = FarmHUDMoistureStatus::None;
	bool cropPieOpen = false;
	farm::CropType cropPieHovered = farm::CropType::None;
	Vector2 cropPieCenter{};
	FarmHUDNextAction nextAction = FarmHUDNextAction::SelectTile;
	FarmHUDFeedback feedback = FarmHUDFeedback::None;
	farm::CropType feedbackCrop = farm::CropType::None;
	int feedbackQualityScore = 0;
	int feedbackSaleCount = 0;
	int feedbackSaleValue = 0;
};

// Presentation-only Farm overlay. Gameplay state remains owned by Farm Systems.
class FarmHUD final {
public:
	bool Initialize(SpriteCommon* spriteCommon);
	void SetViewData(const FarmHUDViewData& viewData);
	void Update(float deltaTime);
	void Draw();

private:
	bool InitializePanels(SpriteCommon* spriteCommon);
	bool InitializeLocalizedSprites(SpriteCommon* spriteCommon);
	bool LoadLocalizedTextureHandles();
	void ApplyLayout();
	void RefreshAllText();
	void UpdateDayText();
	void UpdateMoneyText();
	void UpdateRankText();
	void UpdateCropCountText();
	void UpdateTimeScaleText();
	void UpdateGoalText();
	void UpdateSelectedTileText();
	void UpdateLocalizedSelections();
	void UpdateFeedbackDetails();
	void UpdateCropPieStats();
	void UpdateCropPieMenu();
	void UpdateGoalBar();
	void UpdateToolSelection();

private:
	FarmHUDViewData viewData_;
	bool initialized_ = false;

	BitmapFont font_;
	Sprite statusPanel_;
	Sprite economyPanel_;
	Sprite selectedTilePanel_;
	Sprite toolPanel_;
	Sprite feedbackPanel_;
	Sprite goalBarTrack_;
	Sprite goalBarFill_;
	Sprite cropPieCenterPanel_;
	std::array<Sprite, 2> cropPiePanels_;
	std::array<Sprite, 4> toolSlotPanels_;
	std::array<Sprite, 20> localizedLabels_;
	std::array<Sprite, 4> localizedToolSlotLabels_;
	Sprite localizedCurrentTool_;
	Sprite localizedTileState_;
	Sprite localizedCropName_;
	Sprite localizedSelectedSeedCrop_;
	std::array<Sprite, 2> localizedCropPieNames_;
	std::array<Sprite, 2> localizedCropPieTraits_;
	Sprite localizedCropPieGuide_;
	Sprite localizedNextAction_;
	Sprite localizedMoistureStatus_;
	Sprite localizedIrrigationStatus_;
	Sprite localizedIrrigationPreview_;
	Sprite localizedFeedback_;
	Sprite localizedFeedbackCrop_;

	std::array<Texture2DHandle, 4> currentToolTextureHandles_;
	std::array<Texture2DHandle, 11> tileStateTextureHandles_;
	std::array<Texture2DHandle, 3> cropTextureHandles_;
	std::array<Texture2DHandle, 10> nextActionTextureHandles_;
	std::array<Texture2DHandle, 3> moistureStatusTextureHandles_;
	std::array<Texture2DHandle, 1> irrigationStatusTextureHandles_;
	std::array<Texture2DHandle, 1> irrigationPreviewTextureHandles_;
	std::array<Texture2DHandle, 13> feedbackTextureHandles_;

	SpriteText dayText_;
	SpriteText moneyText_;
	SpriteText rankText_;
	SpriteText cropCountText_;
	SpriteText seedCountText_;
	SpriteText seedPriceText_;
	SpriteText timeScaleText_;
	SpriteText goalText_;
	SpriteText goalNeedText_;
	SpriteText saleText_;
	SpriteText selectedTileTitleText_;
	SpriteText selectedTileMetricsText_;
	SpriteText selectedTileGrowthText_;
	SpriteText feedbackMetricsText_;
	std::array<SpriteText, farm::kFarmCropTypeCount> cropPieStatsText_;
	std::array<SpriteText, farm::kFarmCropTypeCount> cropPieValueText_;
};
