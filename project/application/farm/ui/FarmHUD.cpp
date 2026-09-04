#include "farm/ui/FarmHUD.h"

#include "2d/SpriteCommon.h"
#include "base/Logger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace {
constexpr const char* kFontConfigPath = "Resources/ui/font/ascii_bitmap_font.json";
constexpr const char* kPanelTexturePath = "Resources/ui/hud_panel_fill.png";

constexpr Vector2 kStatusPanelPosition{ 24.0f, 24.0f };
constexpr Vector2 kStatusPanelSize{ 310.0f, 132.0f };
constexpr Vector2 kEconomyPanelPosition{ 876.0f, 24.0f };
constexpr Vector2 kEconomyPanelSize{ 380.0f, 228.0f };
constexpr Vector2 kSelectedPanelPosition{ 24.0f, 520.0f };
constexpr Vector2 kSelectedPanelSize{ 350.0f, 176.0f };
constexpr Vector2 kToolPanelPosition{ 390.0f, 548.0f };
constexpr Vector2 kToolPanelSize{ 628.0f, 148.0f };
constexpr Vector2 kFeedbackPanelPosition{ 400.0f, 264.0f };
constexpr Vector2 kFeedbackPanelSize{ 480.0f, 58.0f };
constexpr Vector2 kGoalTrackPosition{ 900.0f, 136.0f };
constexpr Vector2 kGoalTrackSize{ 332.0f, 12.0f };
constexpr Vector2 kToolSlotSize{ 140.0f, 58.0f };
constexpr float kToolSlotStartX = 408.0f;
constexpr float kToolSlotY = 622.0f;
constexpr float kToolSlotStepX = 148.0f;

constexpr Vector2 kDayPosition{ 118.0f, 40.0f };
constexpr Vector2 kRankPosition{ 166.0f, 88.0f };
constexpr Vector2 kTimeScalePosition{ 166.0f, 117.0f };
constexpr Vector2 kMoneyPosition{ 1022.0f, 38.0f };
constexpr Vector2 kGoalPosition{ 970.0f, 82.0f };
constexpr Vector2 kGoalNeedPosition{ 968.0f, 109.0f };
constexpr Vector2 kCropPosition{ 1008.0f, 160.0f };
constexpr Vector2 kSalePosition{ 1170.0f, 160.0f };
constexpr Vector2 kSeedCountPosition{ 1134.0f, 190.0f };
constexpr Vector2 kSeedPricePosition{ 1190.0f, 218.0f };
constexpr Vector2 kSelectedTitlePosition{ 156.0f, 536.0f };
constexpr Vector2 kSelectedMetricsPosition{ 105.0f, 614.0f };
constexpr Vector2 kSelectedGrowthPosition{ 246.0f, 614.0f };

constexpr float kDayScale = 1.15f;
constexpr float kMoneyScale = 1.20f;
constexpr float kPrimarySpacing = -9.0f;
constexpr float kSecondaryScale = 0.76f;
constexpr float kSecondarySpacing = -8.0f;
constexpr float kSelectedTitleScale = 0.70f;
constexpr float kSelectedTitleSpacing = -8.0f;
constexpr float kSelectedDetailScale = 0.63f;
constexpr float kSelectedDetailSpacing = -7.0f;
constexpr float kCompactScale = 0.66f;
constexpr float kCompactSpacing = -7.0f;
constexpr float kTimeScaleEpsilon = 0.001f;

constexpr Vector4 kPanelColor{ 0.018f, 0.027f, 0.024f, 0.88f };
constexpr Vector4 kPanelSecondaryColor{ 0.025f, 0.045f, 0.038f, 0.86f };
constexpr Vector4 kFeedbackPanelColor{ 0.035f, 0.044f, 0.038f, 0.94f };
constexpr Vector4 kTrackColor{ 0.12f, 0.15f, 0.13f, 0.95f };
constexpr Vector4 kGoalFillColor{ 0.36f, 0.78f, 0.42f, 1.0f };
constexpr Vector4 kToolSlotColor{ 0.09f, 0.12f, 0.11f, 0.96f };
constexpr Vector4 kToolSlotSelectedColor{ 0.88f, 0.62f, 0.08f, 1.0f };
constexpr Vector4 kTextColor{ 0.96f, 0.97f, 0.94f, 1.0f };
constexpr Vector4 kMutedTextColor{ 0.68f, 0.76f, 0.70f, 1.0f };
constexpr Vector4 kAccentColor{ 1.0f, 0.82f, 0.18f, 1.0f };
constexpr Vector4 kSelectedSlotTextColor{ 0.06f, 0.07f, 0.06f, 1.0f };
constexpr Vector4 kGoalTextColor{ 0.72f, 1.0f, 0.72f, 1.0f };
constexpr Vector4 kWaterTextColor{ 0.55f, 0.84f, 1.0f, 1.0f };
constexpr Vector4 kPiePanelColor{ 0.035f, 0.055f, 0.045f, 0.97f };
constexpr Vector4 kPieCurrentColor{ 0.18f, 0.38f, 0.22f, 0.98f };
constexpr Vector4 kPieHoveredColor{ 0.94f, 0.68f, 0.10f, 1.0f };
constexpr Vector2 kPiePanelSize{ 174.0f, 126.0f };
constexpr float kPiePanelGap = 20.0f;

enum LocalizedLabelIndex : std::size_t {
	kDayLabel,
	kRankLabel,
	kSpeedLabel,
	kMoneyLabel,
	kGoalLabel,
	kNeedLabel,
	kNeedSuffixLabel,
	kCropCountLabel,
	kSeedCountLabel,
	kSaleLabel,
	kSellLabel,
	kBuySeedLabel,
	kSelectedSeedLabel,
	kSelectedTileLabel,
	kCropLabel,
	kWaterLabel,
	kGrowthLabel,
	kNextLabel,
	kToolLabel,
	kToolGuideLabel,
};

constexpr std::array<const char*, 20> kLocalizedLabelPaths = {
	"Resources/generated/text/hud_day_label.png",
	"Resources/generated/text/hud_rank_label.png",
	"Resources/generated/text/hud_speed_label.png",
	"Resources/generated/text/hud_money_label.png",
	"Resources/generated/text/hud_goal_label.png",
	"Resources/generated/text/hud_need_label.png",
	"Resources/generated/text/hud_need_suffix_label.png",
	"Resources/generated/text/hud_crop_count_label.png",
	"Resources/generated/text/hud_seed_count_label.png",
	"Resources/generated/text/hud_sale_label.png",
	"Resources/generated/text/hud_sell_label.png",
	"Resources/generated/text/hud_buy_seed_label.png",
	"Resources/generated/text/hud_selected_seed_label.png",
	"Resources/generated/text/hud_selected_tile_label.png",
	"Resources/generated/text/hud_crop_label.png",
	"Resources/generated/text/hud_water_label.png",
	"Resources/generated/text/hud_growth_label.png",
	"Resources/generated/text/hud_next_label.png",
	"Resources/generated/text/hud_tool_label.png",
	"Resources/generated/text/hud_tool_guide_label.png",
};

constexpr std::array<Vector2, 20> kLocalizedLabelPositions = {
	Vector2{ 44.0f, 38.0f }, Vector2{ 44.0f, 82.0f },
	Vector2{ 44.0f, 114.0f }, Vector2{ 900.0f, 36.0f },
	Vector2{ 900.0f, 80.0f }, Vector2{ 900.0f, 107.0f },
	Vector2{ 1022.0f, 107.0f }, Vector2{ 900.0f, 158.0f },
	Vector2{ 1080.0f, 188.0f }, Vector2{ 1070.0f, 158.0f },
	Vector2{ 900.0f, 216.0f }, Vector2{ 1090.0f, 216.0f },
	Vector2{ 900.0f, 188.0f },
	Vector2{ 42.0f, 532.0f }, Vector2{ 42.0f, 572.0f },
	Vector2{ 42.0f, 610.0f }, Vector2{ 180.0f, 610.0f },
	Vector2{ 42.0f, 650.0f }, Vector2{ 412.0f, 560.0f },
	Vector2{ 706.0f, 562.0f },
};

constexpr std::array<const char*, 4> kLocalizedToolSlotPaths = {
	"Resources/generated/text/hud_tool_slot_hoe.png",
	"Resources/generated/text/hud_tool_slot_water.png",
	"Resources/generated/text/hud_tool_slot_seed.png",
	"Resources/generated/text/hud_tool_slot_harvest.png",
};

constexpr std::array<const char*, 4> kCurrentToolPaths = {
	"Resources/generated/text/hud_tool_hoe.png",
	"Resources/generated/text/hud_tool_water.png",
	"Resources/generated/text/hud_tool_seed.png",
	"Resources/generated/text/hud_tool_harvest.png",
};

constexpr std::array<const char*, 11> kTileStatePaths = {
	"Resources/generated/text/hud_state_invalid.png",
	"Resources/generated/text/hud_state_canal.png",
	"Resources/generated/text/hud_state_canal_supplied.png",
	"Resources/generated/text/hud_state_water_source.png",
	"Resources/generated/text/hud_state_empty.png",
	"Resources/generated/text/hud_state_tilled.png",
	"Resources/generated/text/hud_state_watered.png",
	"Resources/generated/text/hud_state_sprout.png",
	"Resources/generated/text/hud_state_planted.png",
	"Resources/generated/text/hud_state_almost_ready.png",
	"Resources/generated/text/hud_state_ready.png",
};

constexpr std::array<const char*, 3> kCropPaths = {
	"Resources/generated/text/hud_crop_none.png",
	"Resources/generated/text/hud_crop_test.png",
	"Resources/generated/text/hud_crop_carrot.png",
};

constexpr const char* kCropPieGuidePath =
	"Resources/generated/text/hud_crop_pie_guide.png";

constexpr std::array<const char*, 2> kCropPieTraitPaths = {
	"Resources/generated/text/hud_crop_trait_turnip.png",
	"Resources/generated/text/hud_crop_trait_carrot.png",
};

constexpr std::array<const char*, 10> kNextActionPaths = {
	"Resources/generated/text/hud_next_select.png",
	"Resources/generated/text/hud_next_canal.png",
	"Resources/generated/text/hud_next_water_source.png",
	"Resources/generated/text/hud_next_hoe.png",
	"Resources/generated/text/hud_next_water_or_seed.png",
	"Resources/generated/text/hud_next_buy_seed.png",
	"Resources/generated/text/hud_next_seed.png",
	"Resources/generated/text/hud_next_water.png",
	"Resources/generated/text/hud_next_growing.png",
	"Resources/generated/text/hud_next_harvest.png",
};

constexpr std::array<const char*, 3> kMoistureStatusPaths = {
	"Resources/generated/text/hud_moisture_dry.png",
	"Resources/generated/text/hud_moisture_low.png",
	"Resources/generated/text/hud_moisture_good.png",
};

constexpr std::array<const char*, 1> kIrrigationStatusPaths = {
	"Resources/generated/text/hud_irrigation_active.png",
};

constexpr std::array<const char*, 1> kIrrigationPreviewPaths = {
	"Resources/generated/text/hud_irrigation_preview.png",
};

constexpr std::array<const char*, 13> kFeedbackPaths = {
	"Resources/generated/text/hud_feedback_harvest.png",
	"Resources/generated/text/hud_feedback_harvest.png",
	"Resources/generated/text/hud_feedback_sale.png",
	"Resources/generated/text/hud_feedback_empty_sale.png",
	"Resources/generated/text/hud_feedback_locked.png",
	"Resources/generated/text/hud_feedback_restarted.png",
	"Resources/generated/text/hud_feedback_seed_bought_turnip.png",
	"Resources/generated/text/hud_feedback_seed_bought_carrot.png",
	"Resources/generated/text/hud_feedback_crop_selected_turnip.png",
	"Resources/generated/text/hud_feedback_crop_selected_carrot.png",
	"Resources/generated/text/hud_feedback_no_seed_turnip.png",
	"Resources/generated/text/hud_feedback_no_seed_carrot.png",
	"Resources/generated/text/hud_feedback_no_money.png",
};

bool IsSameFloat(float lhs, float rhs) {
	return std::fabs(lhs - rhs) <= kTimeScaleEpsilon;
}

std::string FormatTimeScaleText(float timeScale) {
	if (IsSameFloat(timeScale, 1.0f)) {
		return "X1";
	}
	if (IsSameFloat(timeScale, 2.0f)) {
		return "X2";
	}
	if (IsSameFloat(timeScale, 4.0f)) {
		return "X4";
	}
	return "X?";
}

template <std::size_t Size>
bool LoadTextureHandles(
	std::array<Texture2DHandle, Size>& handles,
	const std::array<const char*, Size>& paths) {
	TextureManager* textureManager = TextureManager::GetInstance();
	if (textureManager == nullptr || !textureManager->IsInitialized()) {
		return false;
	}
	for (std::size_t i = 0; i < Size; ++i) {
		handles[i] = textureManager->LoadTexture2D(paths[i]);
		if (!handles[i].IsValid()) {
			return false;
		}
	}
	return true;
}

std::size_t GetTileStateTextureIndex(const FarmHUDViewData& viewData) {
	if (!viewData.selectedTileValid) {
		return 0;
	}
	if (viewData.selectedTileFeature == farm::FarmTileFeature::Canal) {
		return viewData.selectedTileIrrigationSupplied ? 2 : 1;
	}
	if (viewData.selectedTileFeature == farm::FarmTileFeature::WaterSource) {
		return 3;
	}
	if (viewData.selectedTileState == farm::FarmTileState::Planted ||
		viewData.selectedTileState == farm::FarmTileState::ReadyToHarvest) {
		switch (viewData.selectedTileGrowthStage) {
		case farm::FarmCropGrowthStage::Sprout:
			return 7;
		case farm::FarmCropGrowthStage::Growing:
			return 8;
		case farm::FarmCropGrowthStage::AlmostReady:
			return 9;
		case farm::FarmCropGrowthStage::Ready:
			return 10;
		case farm::FarmCropGrowthStage::None:
		default:
			return 0;
		}
	}
	switch (viewData.selectedTileState) {
	case farm::FarmTileState::Empty:
		return 4;
	case farm::FarmTileState::Tilled:
		return 5;
	case farm::FarmTileState::Watered:
		return 6;
	case farm::FarmTileState::ReadyToHarvest:
	case farm::FarmTileState::Planted:
		return 0;
	default:
		return 0;
	}
}

std::size_t GetCropTextureIndex(farm::CropType crop) {
	switch (crop) {
	case farm::CropType::TestCrop:
		return 1;
	case farm::CropType::Carrot:
		return 2;
	case farm::CropType::None:
	default:
		return 0;
	}
}

std::size_t GetMoistureStatusTextureIndex(FarmHUDMoistureStatus status) {
	switch (status) {
	case FarmHUDMoistureStatus::Low:
		return 1;
	case FarmHUDMoistureStatus::Good:
		return 2;
	case FarmHUDMoistureStatus::Dry:
	case FarmHUDMoistureStatus::None:
	default:
		return 0;
	}
}

template <typename Enum, std::size_t Size>
std::size_t ToBoundedIndex(Enum value) {
	const std::size_t index = static_cast<std::size_t>(value);
	return index < Size ? index : 0;
}

void ScaleSprite(Sprite& sprite, float scale) {
	const Vector2 naturalSize = sprite.GetSize();
	sprite.SetSize({ naturalSize.x * scale, naturalSize.y * scale });
}

void SetLocalizedTexture(Sprite& sprite, Texture2DHandle textureHandle, float scale) {
	sprite.SetTexture(textureHandle);
	ScaleSprite(sprite, scale);
}

void ConfigurePanel(
	Sprite& panel,
	const Vector2& position,
	const Vector2& size,
	const Vector4& color) {
	panel.SetPosition(position);
	panel.SetSize(size);
	panel.SetColor(color);
}

void ConfigureText(
	SpriteText& text,
	const Vector2& position,
	float scale,
	float spacing,
	const Vector4& color) {
	text.SetPosition(position);
	text.SetScale(scale);
	text.SetCharacterSpacing(spacing);
	text.SetColor(color);
}
}

bool FarmHUD::Initialize(SpriteCommon* spriteCommon) {
	if (spriteCommon == nullptr) {
		Logger::Log("FarmHUD::Initialize failed. SpriteCommon is null.");
		return false;
	}
	if (!font_.InitializeFromJson(spriteCommon, kFontConfigPath)) {
		Logger::Log("FarmHUD::Initialize failed. BitmapFont is unavailable.");
		return false;
	}
	if (!InitializePanels(spriteCommon)) {
		return false;
	}
	if (!InitializeLocalizedSprites(spriteCommon) || !LoadLocalizedTextureHandles()) {
		Logger::Log("FarmHUD::Initialize failed. Japanese HUD textures are unavailable.");
		return false;
	}

	dayText_.Initialize(spriteCommon, &font_);
	moneyText_.Initialize(spriteCommon, &font_);
	rankText_.Initialize(spriteCommon, &font_);
	cropCountText_.Initialize(spriteCommon, &font_);
	seedCountText_.Initialize(spriteCommon, &font_);
	seedPriceText_.Initialize(spriteCommon, &font_);
	timeScaleText_.Initialize(spriteCommon, &font_);
	goalText_.Initialize(spriteCommon, &font_);
	goalNeedText_.Initialize(spriteCommon, &font_);
	saleText_.Initialize(spriteCommon, &font_);
	selectedTileTitleText_.Initialize(spriteCommon, &font_);
	selectedTileMetricsText_.Initialize(spriteCommon, &font_);
	selectedTileGrowthText_.Initialize(spriteCommon, &font_);
	feedbackMetricsText_.Initialize(spriteCommon, &font_);
	for (SpriteText& statsText : cropPieStatsText_) {
		statsText.Initialize(spriteCommon, &font_);
		statsText.SetText("999 / 999");
	}
	for (SpriteText& valueText : cropPieValueText_) {
		valueText.Initialize(spriteCommon, &font_);
		valueText.SetText("999999G");
	}

	ApplyLayout();

	// Prewarm each bounded text pool once so normal HUD changes allocate no GPU resources.
	dayText_.SetText("999");
	moneyText_.SetText("999999G");
	rankText_.SetText("99");
	cropCountText_.SetText("999");
	seedCountText_.SetText("999");
	seedPriceText_.SetText("999G");
	saleText_.SetText("999999G");
	timeScaleText_.SetText("X4");
	goalText_.SetText("999999 / 999999G");
	goalNeedText_.SetText("999");
	selectedTileTitleText_.SetText("#999  H99");
	selectedTileMetricsText_.SetText("100%");
	selectedTileGrowthText_.SetText("100%");
	feedbackMetricsText_.SetText("Q100 999G");

	initialized_ = true;
	RefreshAllText();
	return true;
}

bool FarmHUD::InitializeLocalizedSprites(SpriteCommon* spriteCommon) {
	for (std::size_t i = 0; i < localizedLabels_.size(); ++i) {
		if (!localizedLabels_[i].Initialize(spriteCommon, kLocalizedLabelPaths[i])) {
			return false;
		}
	}
	for (std::size_t i = 0; i < localizedToolSlotLabels_.size(); ++i) {
		if (!localizedToolSlotLabels_[i].Initialize(spriteCommon, kLocalizedToolSlotPaths[i])) {
			return false;
		}
	}
	return localizedCurrentTool_.Initialize(spriteCommon, kCurrentToolPaths[0]) &&
		localizedTileState_.Initialize(spriteCommon, kTileStatePaths[0]) &&
		localizedCropName_.Initialize(spriteCommon, kCropPaths[0]) &&
		localizedSelectedSeedCrop_.Initialize(spriteCommon, kCropPaths[1]) &&
		localizedCropPieNames_[0].Initialize(spriteCommon, kCropPaths[1]) &&
		localizedCropPieNames_[1].Initialize(spriteCommon, kCropPaths[2]) &&
		localizedCropPieTraits_[0].Initialize(spriteCommon, kCropPieTraitPaths[0]) &&
		localizedCropPieTraits_[1].Initialize(spriteCommon, kCropPieTraitPaths[1]) &&
		localizedCropPieGuide_.Initialize(spriteCommon, kCropPieGuidePath) &&
		localizedNextAction_.Initialize(spriteCommon, kNextActionPaths[0]) &&
		localizedMoistureStatus_.Initialize(spriteCommon, kMoistureStatusPaths[0]) &&
		localizedIrrigationStatus_.Initialize(spriteCommon, kIrrigationStatusPaths[0]) &&
		localizedIrrigationPreview_.Initialize(spriteCommon, kIrrigationPreviewPaths[0]) &&
		localizedFeedback_.Initialize(spriteCommon, kFeedbackPaths[0]) &&
		localizedFeedbackCrop_.Initialize(spriteCommon, kCropPaths[0]);
}

bool FarmHUD::LoadLocalizedTextureHandles() {
	return LoadTextureHandles(currentToolTextureHandles_, kCurrentToolPaths) &&
		LoadTextureHandles(tileStateTextureHandles_, kTileStatePaths) &&
		LoadTextureHandles(cropTextureHandles_, kCropPaths) &&
		LoadTextureHandles(nextActionTextureHandles_, kNextActionPaths) &&
		LoadTextureHandles(moistureStatusTextureHandles_, kMoistureStatusPaths) &&
		LoadTextureHandles(irrigationStatusTextureHandles_, kIrrigationStatusPaths) &&
		LoadTextureHandles(irrigationPreviewTextureHandles_, kIrrigationPreviewPaths) &&
		LoadTextureHandles(feedbackTextureHandles_, kFeedbackPaths);
}

bool FarmHUD::InitializePanels(SpriteCommon* spriteCommon) {
	auto initialize = [spriteCommon](Sprite& sprite) {
		return sprite.Initialize(spriteCommon, kPanelTexturePath);
	};
	if (!initialize(statusPanel_) || !initialize(economyPanel_) ||
		!initialize(selectedTilePanel_) || !initialize(toolPanel_) ||
		!initialize(feedbackPanel_) || !initialize(goalBarTrack_) ||
		!initialize(goalBarFill_) || !initialize(cropPieCenterPanel_)) {
		Logger::Log("FarmHUD::Initialize failed. HUD panel texture is unavailable.");
		return false;
	}
	for (Sprite& panel : toolSlotPanels_) {
		if (!initialize(panel)) {
			Logger::Log("FarmHUD::Initialize failed. Tool slot panel is unavailable.");
			return false;
		}
	}
	for (Sprite& panel : cropPiePanels_) {
		if (!initialize(panel)) {
			Logger::Log("FarmHUD::Initialize failed. Crop Pie Menu panel is unavailable.");
			return false;
		}
	}
	return true;
}

void FarmHUD::SetViewData(const FarmHUDViewData& viewData) {
	FarmHUDViewData sanitized = viewData;
	sanitized.goalMoney = (std::max)(sanitized.goalMoney, 1);
	sanitized.goalProgress = std::isfinite(sanitized.goalProgress)
		? std::clamp(sanitized.goalProgress, 0.0f, 1.0f) : 0.0f;
	if (sanitized.currentToolIndex < 0 || sanitized.currentToolIndex >= 4) {
		sanitized.currentToolIndex = -1;
	}
	if (!farm::IsPlantableCrop(sanitized.selectedSeedCrop)) {
		sanitized.selectedSeedCrop = farm::CropType::TestCrop;
	}
	if (!farm::IsPlantableCrop(sanitized.cropPieHovered)) {
		sanitized.cropPieHovered = farm::CropType::None;
	}
	if (sanitized.feedback == FarmHUDFeedback::Harvest &&
		farm::IsPlantableCrop(sanitized.feedbackCrop)) {
		sanitized.feedbackQualityScore =
			std::clamp(sanitized.feedbackQualityScore, 0, 100);
		sanitized.feedbackSaleCount = 0;
		sanitized.feedbackSaleValue = (std::max)(sanitized.feedbackSaleValue, 0);
	} else if (sanitized.feedback == FarmHUDFeedback::Sale) {
		if (!farm::IsPlantableCrop(sanitized.feedbackCrop)) {
			sanitized.feedbackCrop = farm::CropType::None;
		}
		sanitized.feedbackQualityScore = 0;
		sanitized.feedbackSaleCount = (std::max)(sanitized.feedbackSaleCount, 0);
		sanitized.feedbackSaleValue = (std::max)(sanitized.feedbackSaleValue, 0);
	} else {
		sanitized.feedbackCrop = farm::CropType::None;
		sanitized.feedbackQualityScore = 0;
		sanitized.feedbackSaleCount = 0;
		sanitized.feedbackSaleValue = 0;
	}
	if (!std::isfinite(sanitized.cropPieCenter.x) ||
		!std::isfinite(sanitized.cropPieCenter.y)) {
		sanitized.cropPieOpen = false;
		sanitized.cropPieCenter = {};
	}
	if (!sanitized.selectedTileValid || sanitized.selectedTileIndex < 0) {
		sanitized.selectedTileValid = false;
		sanitized.selectedTileIndex = -1;
		sanitized.selectedTileHeight = 0;
		sanitized.selectedTileMoisturePercent = 0;
		sanitized.selectedTileGrowthPercent = 0;
		sanitized.selectedTileFeature = farm::FarmTileFeature::None;
		sanitized.selectedTileIrrigationSupplied = false;
		sanitized.selectedTileIrrigationActive = false;
		sanitized.selectedTileIrrigationStrengthPercent = 0;
		sanitized.selectedTileCrop = farm::CropType::None;
		sanitized.selectedTileGrowthStage = farm::FarmCropGrowthStage::None;
		sanitized.selectedTileMoistureStatus = FarmHUDMoistureStatus::None;
		sanitized.nextAction = FarmHUDNextAction::SelectTile;
	} else {
		if (!farm::IsValidFarmTileFeature(sanitized.selectedTileFeature)) {
			sanitized.selectedTileFeature = farm::FarmTileFeature::None;
			sanitized.selectedTileIrrigationSupplied = false;
			sanitized.selectedTileIrrigationActive = false;
		}
		if (sanitized.selectedTileFeature == farm::FarmTileFeature::None) {
			sanitized.selectedTileIrrigationSupplied = false;
		} else {
			sanitized.selectedTileIrrigationActive = false;
		}
		if (sanitized.selectedTileState == farm::FarmTileState::Empty) {
			sanitized.selectedTileIrrigationActive = false;
		}
		sanitized.selectedTileMoisturePercent =
			std::clamp(sanitized.selectedTileMoisturePercent, 0, 100);
		sanitized.selectedTileGrowthPercent =
			std::clamp(sanitized.selectedTileGrowthPercent, 0, 100);
		sanitized.selectedTileIrrigationStrengthPercent =
			std::clamp(sanitized.selectedTileIrrigationStrengthPercent, 0, 100);
		if (sanitized.selectedTileMoistureStatus != FarmHUDMoistureStatus::Dry &&
			sanitized.selectedTileMoistureStatus != FarmHUDMoistureStatus::Low &&
			sanitized.selectedTileMoistureStatus != FarmHUDMoistureStatus::Good) {
			sanitized.selectedTileMoistureStatus = FarmHUDMoistureStatus::None;
		}
	}
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		const std::size_t index = static_cast<std::size_t>(slot);
		sanitized.cropInventoryCounts[index] =
			(std::max)(sanitized.cropInventoryCounts[index], 0);
		sanitized.cropInventoryValues[index] =
			(std::max)(sanitized.cropInventoryValues[index], 0);
		sanitized.cropSeedCounts[index] =
			(std::max)(sanitized.cropSeedCounts[index], 0);
	}

	const bool dayChanged = viewData_.day != sanitized.day;
	const bool moneyChanged = viewData_.money != sanitized.money;
	const bool rankChanged = viewData_.rank != sanitized.rank;
	const bool cropChanged = viewData_.cropCount != sanitized.cropCount ||
		viewData_.saleValue != sanitized.saleValue ||
		viewData_.seedCount != sanitized.seedCount ||
		viewData_.seedPrice != sanitized.seedPrice ||
		viewData_.cropInventoryCounts != sanitized.cropInventoryCounts ||
		viewData_.cropInventoryValues != sanitized.cropInventoryValues ||
		viewData_.cropSeedCounts != sanitized.cropSeedCounts ||
		viewData_.selectedSeedCrop != sanitized.selectedSeedCrop;
	const bool cropPieChanged = viewData_.cropPieOpen != sanitized.cropPieOpen ||
		viewData_.cropPieHovered != sanitized.cropPieHovered ||
		!IsSameFloat(viewData_.cropPieCenter.x, sanitized.cropPieCenter.x) ||
		!IsSameFloat(viewData_.cropPieCenter.y, sanitized.cropPieCenter.y) ||
		viewData_.selectedSeedCrop != sanitized.selectedSeedCrop;
	const bool timeScaleChanged = !IsSameFloat(viewData_.timeScale, sanitized.timeScale);
	const bool toolChanged = viewData_.currentToolIndex != sanitized.currentToolIndex;
	const bool goalChanged = viewData_.money != sanitized.money ||
		viewData_.goalMoney != sanitized.goalMoney ||
		viewData_.cropsNeeded != sanitized.cropsNeeded ||
		viewData_.goalCleared != sanitized.goalCleared ||
		!IsSameFloat(viewData_.goalProgress, sanitized.goalProgress);
	const bool selectedTileChanged =
		viewData_.selectedTileValid != sanitized.selectedTileValid ||
		viewData_.selectedTileIndex != sanitized.selectedTileIndex ||
		viewData_.selectedTileHeight != sanitized.selectedTileHeight ||
		viewData_.selectedTileMoisturePercent != sanitized.selectedTileMoisturePercent ||
		viewData_.selectedTileGrowthPercent != sanitized.selectedTileGrowthPercent ||
		viewData_.selectedTileFeature != sanitized.selectedTileFeature ||
		viewData_.selectedTileIrrigationSupplied != sanitized.selectedTileIrrigationSupplied ||
		viewData_.selectedTileIrrigationActive != sanitized.selectedTileIrrigationActive ||
		viewData_.selectedTileIrrigationStrengthPercent !=
			sanitized.selectedTileIrrigationStrengthPercent ||
		viewData_.irrigationPreviewActive != sanitized.irrigationPreviewActive ||
		viewData_.selectedTileState != sanitized.selectedTileState ||
		viewData_.selectedTileCrop != sanitized.selectedTileCrop ||
		viewData_.selectedTileGrowthStage != sanitized.selectedTileGrowthStage ||
		viewData_.selectedTileMoistureStatus != sanitized.selectedTileMoistureStatus ||
		viewData_.nextAction != sanitized.nextAction;
	const bool feedbackChanged = viewData_.feedback != sanitized.feedback ||
		viewData_.feedbackCrop != sanitized.feedbackCrop ||
		viewData_.feedbackQualityScore != sanitized.feedbackQualityScore ||
		viewData_.feedbackSaleCount != sanitized.feedbackSaleCount ||
		viewData_.feedbackSaleValue != sanitized.feedbackSaleValue;

	viewData_ = std::move(sanitized);
	if (dayChanged) UpdateDayText();
	if (moneyChanged) UpdateMoneyText();
	if (rankChanged) UpdateRankText();
	if (cropChanged) UpdateCropCountText();
	if (timeScaleChanged) UpdateTimeScaleText();
	if (toolChanged) {
		UpdateToolSelection();
	}
	if (goalChanged) {
		UpdateGoalText();
		UpdateGoalBar();
	}
	if (selectedTileChanged || feedbackChanged || toolChanged || cropChanged) {
		UpdateLocalizedSelections();
	}
	if (feedbackChanged) UpdateFeedbackDetails();
	if (cropChanged) UpdateCropPieStats();
	if (cropPieChanged) UpdateCropPieMenu();
	if (selectedTileChanged) UpdateSelectedTileText();
}

void FarmHUD::Update(float deltaTime) {
	(void)deltaTime;
	if (!initialized_) {
		return;
	}

	statusPanel_.Update();
	economyPanel_.Update();
	selectedTilePanel_.Update();
	toolPanel_.Update();
	goalBarTrack_.Update();
	if (viewData_.goalProgress > 0.0f) goalBarFill_.Update();
	for (Sprite& panel : toolSlotPanels_) panel.Update();
	for (Sprite& label : localizedLabels_) label.Update();
	for (Sprite& label : localizedToolSlotLabels_) label.Update();
	localizedCurrentTool_.Update();
	localizedTileState_.Update();
	localizedCropName_.Update();
	localizedSelectedSeedCrop_.Update();
	localizedNextAction_.Update();
	if (viewData_.selectedTileValid &&
		viewData_.selectedTileMoistureStatus != FarmHUDMoistureStatus::None) {
		localizedMoistureStatus_.Update();
	}
	if (viewData_.selectedTileIrrigationActive) {
		localizedIrrigationStatus_.Update();
	}
	if (viewData_.irrigationPreviewActive) {
		localizedIrrigationPreview_.Update();
	}
	if (viewData_.cropPieOpen) {
		cropPieCenterPanel_.Update();
		for (Sprite& panel : cropPiePanels_) panel.Update();
		for (Sprite& name : localizedCropPieNames_) name.Update();
		for (Sprite& trait : localizedCropPieTraits_) trait.Update();
		localizedCropPieGuide_.Update();
	}
	if (viewData_.feedback != FarmHUDFeedback::None) {
		feedbackPanel_.Update();
		localizedFeedback_.Update();
		if (viewData_.feedback == FarmHUDFeedback::Harvest &&
			farm::IsPlantableCrop(viewData_.feedbackCrop)) {
			localizedFeedbackCrop_.Update();
	feedbackMetricsText_.Update();
	for (SpriteText& statsText : cropPieStatsText_) statsText.Update();
	for (SpriteText& valueText : cropPieValueText_) valueText.Update();
		}
	}

	dayText_.Update();
	moneyText_.Update();
	rankText_.Update();
	cropCountText_.Update();
	seedCountText_.Update();
	seedPriceText_.Update();
	timeScaleText_.Update();
	goalText_.Update();
	goalNeedText_.Update();
	saleText_.Update();
	selectedTileTitleText_.Update();
	selectedTileMetricsText_.Update();
	selectedTileGrowthText_.Update();
}

void FarmHUD::Draw() {
	if (!initialized_) {
		return;
	}

	statusPanel_.Draw();
	economyPanel_.Draw();
	selectedTilePanel_.Draw();
	toolPanel_.Draw();
	goalBarTrack_.Draw();
	if (viewData_.goalProgress > 0.0f) goalBarFill_.Draw();
	for (Sprite& panel : toolSlotPanels_) panel.Draw();
	for (Sprite& label : localizedLabels_) label.Draw();
	for (Sprite& label : localizedToolSlotLabels_) label.Draw();
	localizedCurrentTool_.Draw();
	localizedTileState_.Draw();
	localizedCropName_.Draw();
	localizedSelectedSeedCrop_.Draw();
	localizedNextAction_.Draw();
	if (viewData_.selectedTileValid &&
		viewData_.selectedTileMoistureStatus != FarmHUDMoistureStatus::None) {
		localizedMoistureStatus_.Draw();
	}
	if (viewData_.selectedTileIrrigationActive) {
		localizedIrrigationStatus_.Draw();
	}
	if (viewData_.irrigationPreviewActive) {
		localizedIrrigationPreview_.Draw();
	}
	if (viewData_.cropPieOpen) {
		cropPieCenterPanel_.Draw();
		for (Sprite& panel : cropPiePanels_) panel.Draw();
		for (Sprite& name : localizedCropPieNames_) name.Draw();
		for (Sprite& trait : localizedCropPieTraits_) trait.Draw();
		for (SpriteText& statsText : cropPieStatsText_) statsText.Draw();
		for (SpriteText& valueText : cropPieValueText_) valueText.Draw();
		localizedCropPieGuide_.Draw();
	}
	if (viewData_.feedback != FarmHUDFeedback::None) {
		feedbackPanel_.Draw();
		localizedFeedback_.Draw();
		if ((viewData_.feedback == FarmHUDFeedback::Harvest ||
			viewData_.feedback == FarmHUDFeedback::Sale) &&
			farm::IsPlantableCrop(viewData_.feedbackCrop)) {
			localizedFeedbackCrop_.Draw();
		}
		feedbackMetricsText_.Draw();
	}

	dayText_.Draw();
	moneyText_.Draw();
	rankText_.Draw();
	cropCountText_.Draw();
	seedCountText_.Draw();
	seedPriceText_.Draw();
	timeScaleText_.Draw();
	goalText_.Draw();
	goalNeedText_.Draw();
	saleText_.Draw();
	selectedTileTitleText_.Draw();
	selectedTileMetricsText_.Draw();
	selectedTileGrowthText_.Draw();
}

void FarmHUD::ApplyLayout() {
	ConfigurePanel(statusPanel_, kStatusPanelPosition, kStatusPanelSize, kPanelColor);
	ConfigurePanel(economyPanel_, kEconomyPanelPosition, kEconomyPanelSize, kPanelColor);
	ConfigurePanel(selectedTilePanel_, kSelectedPanelPosition, kSelectedPanelSize, kPanelSecondaryColor);
	ConfigurePanel(toolPanel_, kToolPanelPosition, kToolPanelSize, kPanelColor);
	ConfigurePanel(feedbackPanel_, kFeedbackPanelPosition, kFeedbackPanelSize, kFeedbackPanelColor);
	ConfigurePanel(goalBarTrack_, kGoalTrackPosition, kGoalTrackSize, kTrackColor);
	ConfigurePanel(goalBarFill_, kGoalTrackPosition, { 0.0f, kGoalTrackSize.y }, kGoalFillColor);
	for (std::size_t i = 0; i < toolSlotPanels_.size(); ++i) {
		ConfigurePanel(toolSlotPanels_[i],
			{ kToolSlotStartX + static_cast<float>(i) * kToolSlotStepX, kToolSlotY },
			kToolSlotSize, kToolSlotColor);
	}
	for (std::size_t i = 0; i < localizedLabels_.size(); ++i) {
		localizedLabels_[i].SetPosition(kLocalizedLabelPositions[i]);
		ScaleSprite(localizedLabels_[i], 0.72f);
	}
	for (std::size_t i = 0; i < localizedToolSlotLabels_.size(); ++i) {
		localizedToolSlotLabels_[i].SetPosition({
			kToolSlotStartX + 12.0f + static_cast<float>(i) * kToolSlotStepX,
			kToolSlotY + 10.0f });
		ScaleSprite(localizedToolSlotLabels_[i], 0.78f);
	}
	localizedCurrentTool_.SetPosition({ 482.0f, 558.0f });
	localizedTileState_.SetPosition({ 238.0f, 530.0f });
	localizedCropName_.SetPosition({ 108.0f, 570.0f });
	localizedSelectedSeedCrop_.SetPosition({ 988.0f, 188.0f });
	localizedNextAction_.SetPosition({ 94.0f, 648.0f });
	localizedMoistureStatus_.SetPosition({ 272.0f, 572.0f });
	localizedIrrigationStatus_.SetPosition({ 272.0f, 592.0f });
	localizedIrrigationPreview_.SetPosition({ 500.0f, 34.0f });
	localizedFeedback_.SetPosition({ 416.0f, 272.0f });
	localizedFeedbackCrop_.SetPosition({ 566.0f, 272.0f });
	ScaleSprite(localizedCropPieGuide_, 0.72f);
	for (Sprite& trait : localizedCropPieTraits_) {
		ScaleSprite(trait, 0.72f);
	}

	ConfigureText(dayText_, kDayPosition, kDayScale, kPrimarySpacing, kTextColor);
	ConfigureText(moneyText_, kMoneyPosition, kMoneyScale, kPrimarySpacing, kAccentColor);
	ConfigureText(rankText_, kRankPosition, kSecondaryScale, kSecondarySpacing, kMutedTextColor);
	ConfigureText(cropCountText_, kCropPosition, kCompactScale, kCompactSpacing, kTextColor);
	ConfigureText(seedCountText_, kSeedCountPosition, kCompactScale, kCompactSpacing, kTextColor);
	ConfigureText(seedPriceText_, kSeedPricePosition, kCompactScale, kCompactSpacing, kAccentColor);
	ConfigureText(timeScaleText_, kTimeScalePosition, kCompactScale, kCompactSpacing, kMutedTextColor);
	ConfigureText(goalText_, kGoalPosition, kCompactScale, kCompactSpacing, kGoalTextColor);
	ConfigureText(goalNeedText_, kGoalNeedPosition, kCompactScale, kCompactSpacing, kMutedTextColor);
	ConfigureText(saleText_, kSalePosition, kCompactScale, kCompactSpacing, kTextColor);
	ConfigureText(
		selectedTileTitleText_,
		kSelectedTitlePosition,
		kSelectedTitleScale,
		kSelectedTitleSpacing,
		kTextColor);
	ConfigureText(
		selectedTileMetricsText_,
		kSelectedMetricsPosition,
		kSelectedDetailScale,
		kSelectedDetailSpacing,
		kWaterTextColor);
	ConfigureText(
		selectedTileGrowthText_,
		kSelectedGrowthPosition,
		kSelectedDetailScale,
		kSelectedDetailSpacing,
		kGoalTextColor);
	ConfigureText(
		feedbackMetricsText_,
		{ 710.0f, 276.0f },
		0.70f,
		-7.0f,
		kAccentColor);
}

void FarmHUD::RefreshAllText() {
	UpdateDayText();
	UpdateMoneyText();
	UpdateRankText();
	UpdateCropCountText();
	UpdateTimeScaleText();
	UpdateGoalText();
	UpdateSelectedTileText();
	UpdateLocalizedSelections();
	UpdateFeedbackDetails();
	UpdateCropPieStats();
	UpdateCropPieMenu();
	UpdateGoalBar();
	UpdateToolSelection();
}

void FarmHUD::UpdateDayText() {
	dayText_.SetText(std::to_string((std::max)(viewData_.day, 1)));
}

void FarmHUD::UpdateMoneyText() {
	moneyText_.SetText(std::to_string((std::max)(viewData_.money, 0)) + "G");
}

void FarmHUD::UpdateRankText() {
	rankText_.SetText(std::to_string((std::max)(viewData_.rank, 1)));
}

void FarmHUD::UpdateCropCountText() {
	cropCountText_.SetText(std::to_string((std::max)(viewData_.cropCount, 0)));
	seedCountText_.SetText(std::to_string((std::max)(viewData_.seedCount, 0)));
	seedPriceText_.SetText(std::to_string((std::max)(viewData_.seedPrice, 0)) + "G");
	saleText_.SetText(std::to_string((std::max)(viewData_.saleValue, 0)) + "G");
}

void FarmHUD::UpdateTimeScaleText() {
	timeScaleText_.SetText(FormatTimeScaleText(viewData_.timeScale));
}

void FarmHUD::UpdateGoalText() {
	if (viewData_.goalCleared) {
		goalText_.SetText(std::to_string(viewData_.goalMoney) + "G");
		goalNeedText_.SetText("0");
		return;
	}
	goalText_.SetText(
		std::to_string((std::max)(viewData_.money, 0)) + " / " +
		std::to_string(viewData_.goalMoney) + "G");
	goalNeedText_.SetText(std::to_string((std::max)(viewData_.cropsNeeded, 0)));
}

void FarmHUD::UpdateSelectedTileText() {
	if (!viewData_.selectedTileValid) {
		selectedTileTitleText_.SetText("--");
		selectedTileMetricsText_.SetText("--");
		selectedTileGrowthText_.SetText("--");
		return;
	}
	selectedTileTitleText_.SetText(
		"#" + std::to_string(viewData_.selectedTileIndex) +
		"  H" + std::to_string(viewData_.selectedTileHeight));
	const int waterPercent = viewData_.selectedTileFeature == farm::FarmTileFeature::None
		? viewData_.selectedTileMoisturePercent
		: viewData_.selectedTileIrrigationStrengthPercent;
	selectedTileMetricsText_.SetText(std::to_string(waterPercent) + "%");
	selectedTileGrowthText_.SetText(
		std::to_string(viewData_.selectedTileGrowthPercent) + "%");
}

void FarmHUD::UpdateLocalizedSelections() {
	const std::size_t currentToolIndex = viewData_.currentToolIndex >= 0
		? static_cast<std::size_t>(viewData_.currentToolIndex) : 0;
	SetLocalizedTexture(
		localizedCurrentTool_,
		currentToolTextureHandles_[(std::min)(currentToolIndex, currentToolTextureHandles_.size() - 1)],
		0.75f);
	SetLocalizedTexture(
		localizedTileState_, tileStateTextureHandles_[GetTileStateTextureIndex(viewData_)], 0.75f);
	SetLocalizedTexture(
		localizedCropName_, cropTextureHandles_[GetCropTextureIndex(viewData_.selectedTileCrop)], 0.75f);
	SetLocalizedTexture(
		localizedSelectedSeedCrop_,
		cropTextureHandles_[GetCropTextureIndex(viewData_.selectedSeedCrop)], 0.72f);
	SetLocalizedTexture(
		localizedNextAction_,
		nextActionTextureHandles_[ToBoundedIndex<FarmHUDNextAction, 10>(viewData_.nextAction)],
		0.75f);
	if (viewData_.selectedTileMoistureStatus != FarmHUDMoistureStatus::None) {
		SetLocalizedTexture(
			localizedMoistureStatus_,
			moistureStatusTextureHandles_[GetMoistureStatusTextureIndex(
				viewData_.selectedTileMoistureStatus)],
			0.72f);
	}
	SetLocalizedTexture(
		localizedIrrigationStatus_, irrigationStatusTextureHandles_[0], 0.70f);
	SetLocalizedTexture(
		localizedIrrigationPreview_, irrigationPreviewTextureHandles_[0], 0.86f);
	SetLocalizedTexture(
		localizedFeedback_,
		feedbackTextureHandles_[ToBoundedIndex<FarmHUDFeedback, 13>(viewData_.feedback)],
		0.72f);
	SetLocalizedTexture(
		localizedFeedbackCrop_,
		cropTextureHandles_[GetCropTextureIndex(viewData_.feedbackCrop)],
		0.72f);
}

void FarmHUD::UpdateFeedbackDetails() {
	if (viewData_.feedback == FarmHUDFeedback::Harvest &&
		farm::IsPlantableCrop(viewData_.feedbackCrop)) {
		feedbackMetricsText_.SetText(
			"Q" + std::to_string(viewData_.feedbackQualityScore) + "  " +
			std::to_string(viewData_.feedbackSaleValue) + "G");
		return;
	}
	if (viewData_.feedback == FarmHUDFeedback::Sale &&
		viewData_.feedbackSaleCount > 0 && viewData_.feedbackSaleValue > 0) {
		feedbackMetricsText_.SetText(
			"x" + std::to_string(viewData_.feedbackSaleCount) + "  +" +
			std::to_string(viewData_.feedbackSaleValue) + "G");
		return;
	}
	feedbackMetricsText_.SetText("");
}

void FarmHUD::UpdateCropPieStats() {
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		const std::size_t index = static_cast<std::size_t>(slot);
		cropPieStatsText_[index].SetText(
			std::to_string(viewData_.cropSeedCounts[index]) + " / " +
			std::to_string(viewData_.cropInventoryCounts[index]));
		cropPieValueText_[index].SetText(
			std::to_string(viewData_.cropInventoryValues[index]) + "G");
	}
}

void FarmHUD::UpdateCropPieMenu() {
	const Vector2 center = viewData_.cropPieCenter;
	const float halfGap = kPiePanelGap * 0.5f;
	const Vector2 positions[2] = {
		{ center.x - halfGap - kPiePanelSize.x, center.y - kPiePanelSize.y * 0.5f },
		{ center.x + halfGap, center.y - kPiePanelSize.y * 0.5f },
	};
	const farm::CropType crops[2] = {
		farm::CropType::TestCrop,
		farm::CropType::Carrot,
	};
	for (std::size_t index = 0; index < cropPiePanels_.size(); ++index) {
		Vector4 color = crops[index] == viewData_.selectedSeedCrop
			? kPieCurrentColor : kPiePanelColor;
		if (crops[index] == viewData_.cropPieHovered) {
			color = kPieHoveredColor;
		}
		ConfigurePanel(cropPiePanels_[index], positions[index], kPiePanelSize, color);
		SetLocalizedTexture(
			localizedCropPieNames_[index],
			cropTextureHandles_[GetCropTextureIndex(crops[index])],
			0.95f);
		localizedCropPieNames_[index].SetPosition({
			positions[index].x + 30.0f,
			positions[index].y + 12.0f });
		localizedCropPieNames_[index].SetColor(
			crops[index] == viewData_.cropPieHovered
				? kSelectedSlotTextColor : kTextColor);
		localizedCropPieTraits_[index].SetPosition({
			positions[index].x + 18.0f,
			positions[index].y + 46.0f });
		localizedCropPieTraits_[index].SetColor(
			crops[index] == viewData_.cropPieHovered
				? kSelectedSlotTextColor : kMutedTextColor);
		ConfigureText(
			cropPieStatsText_[index],
			{ positions[index].x + 18.0f, positions[index].y + 80.0f },
			0.60f,
			-7.0f,
			crops[index] == viewData_.cropPieHovered
				? kSelectedSlotTextColor : kTextColor);
		ConfigureText(
			cropPieValueText_[index],
			{ positions[index].x + 18.0f, positions[index].y + 102.0f },
			0.56f,
			-7.0f,
			crops[index] == viewData_.cropPieHovered
				? kSelectedSlotTextColor : kAccentColor);
	}
	ConfigurePanel(
		cropPieCenterPanel_,
		{ center.x - halfGap, center.y - 18.0f },
		{ kPiePanelGap, 36.0f },
		kFeedbackPanelColor);
	localizedCropPieGuide_.SetPosition({ center.x - 160.0f, center.y + 76.0f });
}

void FarmHUD::UpdateGoalBar() {
	const float width = kGoalTrackSize.x * std::clamp(viewData_.goalProgress, 0.0f, 1.0f);
	goalBarFill_.SetSize({ width, kGoalTrackSize.y });
	goalBarFill_.SetColor(viewData_.goalCleared ? kAccentColor : kGoalFillColor);
}

void FarmHUD::UpdateToolSelection() {
	for (std::size_t i = 0; i < toolSlotPanels_.size(); ++i) {
		const bool selected = static_cast<int>(i) == viewData_.currentToolIndex;
		toolSlotPanels_[i].SetColor(selected ? kToolSlotSelectedColor : kToolSlotColor);
		localizedToolSlotLabels_[i].SetColor(selected ? kSelectedSlotTextColor : kTextColor);
	}
}
