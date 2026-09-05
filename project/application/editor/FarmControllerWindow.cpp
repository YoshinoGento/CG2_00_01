#include "editor/FarmControllerWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"

#include <algorithm>
#include <array>
#include <cstdio>
#endif

namespace {
#ifdef USE_IMGUI
struct ToolButtonData {
	FarmTool tool;
	const char* label;
};

constexpr std::array<ToolButtonData, 4> kToolButtons = {{
	{ FarmTool::Hoe, "1  Hoe" },
	{ FarmTool::Water, "2  Water" },
	{ FarmTool::Seed, "3  Seed" },
	{ FarmTool::Harvest, "4  Harvest" },
}};

const char* GetActionStatusLabel(FarmToolActionStatus status, EditorLanguage language)
{
	const char* label = "Tool does not match tile state";
	switch (status) {
	case FarmToolActionStatus::Applied:
	case FarmToolActionStatus::Harvested:
		label = "Action available";
		break;
	case FarmToolActionStatus::AlreadyWatered:
		label = "Already fully watered";
		break;
	case FarmToolActionStatus::NoSeed:
		label = "No seed in inventory";
		break;
	case FarmToolActionStatus::NotReady:
		label = "Crop is not ready";
		break;
	case FarmToolActionStatus::InvalidTarget:
		label = "No valid tile";
		break;
	case FarmToolActionStatus::UnsupportedTool:
		label = "Tool is unavailable";
		break;
	case FarmToolActionStatus::InvalidState:
	default:
		break;
	}
	return editor::Localize(language, label);
}

const char* GetStateLabel(
	const editor::FarmTileEditorViewData& tile,
	EditorLanguage language) noexcept {
	if (tile.feature == farm::FarmTileFeature::Canal) {
		return editor::Localize(language, tile.irrigationSupplied ? "Supplied" : "Dry Canal");
	}
	if (tile.feature == farm::FarmTileFeature::WaterSource) {
		return editor::Localize(language, "Water Source");
	}
	const char* label = farm::ToString(tile.state);
	if (tile.canHarvest) {
		label = "Ready";
	} else if (tile.state == farm::FarmTileState::Planted) {
		switch (tile.growthStage) {
		case farm::FarmCropGrowthStage::Sprout:
			label = "Sprout";
			break;
		case farm::FarmCropGrowthStage::AlmostReady:
			label = "Almost Ready";
			break;
		case farm::FarmCropGrowthStage::Ready:
			label = "Ready";
			break;
		case farm::FarmCropGrowthStage::Growing:
		case farm::FarmCropGrowthStage::None:
		default:
			label = "Growing";
			break;
		}
	} else if (tile.state == farm::FarmTileState::Tilled && tile.moisture > 0.5f) {
		label = "Watered";
	}
	return editor::Localize(language, label);
}

ImVec4 GetStateColor(const editor::FarmTileEditorViewData& tile) noexcept {
	if (tile.feature == farm::FarmTileFeature::Canal) {
		return tile.irrigationSupplied
			? ImVec4(0.10f, 0.78f, 1.0f, 1.0f)
			: ImVec4(0.10f, 0.34f, 0.54f, 1.0f);
	}
	if (tile.feature == farm::FarmTileFeature::WaterSource) {
		return { 0.14f, 0.92f, 1.0f, 1.0f };
	}
	if (tile.canHarvest) {
		return { 0.95f, 0.72f, 0.16f, 1.0f };
	}
	if (tile.state == farm::FarmTileState::Planted) {
		return { 0.28f, 0.78f, 0.34f, 1.0f };
	}
	if (tile.state == farm::FarmTileState::Tilled) {
		return tile.moisture > 0.5f
			? ImVec4(0.30f, 0.58f, 0.88f, 1.0f)
			: ImVec4(0.72f, 0.45f, 0.24f, 1.0f);
	}
	return { 0.40f, 0.72f, 0.32f, 1.0f };
}

const char* GetNextActionLabel(
	const editor::FarmTileEditorViewData& tile,
	EditorLanguage language) noexcept {
	const char* label = "Growing: wait until the crop is ready.";
	if (tile.feature == farm::FarmTileFeature::Canal) {
		label = tile.irrigationSupplied
			? "Supplied from a water source. N removes this canal."
			: "Dry canal: connect it to an equal or higher water source.";
	} else if (tile.feature == farm::FarmTileFeature::WaterSource) {
		label = "Next: connect a canal or press M to remove the source.";
	} else if (tile.canHarvest) {
		label = "Next: select Harvest and apply.";
	} else if (tile.state == farm::FarmTileState::Empty) {
		label = "Next: select Hoe to prepare this tile.";
	} else if (tile.state == farm::FarmTileState::Tilled) {
		label = tile.moisture > 0.5f
			? "Next: select Seed to plant."
			: "Next: Water this tile or plant a Seed.";
	} else if (tile.moisture < 0.25f) {
		label = "Growing slowly: add Water.";
	}
	return editor::Localize(language, label);
}

const char* GetMoistureStatusLabel(
	FarmMoistureStatus status,
	EditorLanguage language) noexcept {
	const char* label = "Unavailable";
	switch (status) {
	case FarmMoistureStatus::Dry:
		label = "Dry";
		break;
	case FarmMoistureStatus::Low:
		label = "Low";
		break;
	case FarmMoistureStatus::Good:
		label = "Good";
		break;
	case FarmMoistureStatus::Invalid:
	default:
		break;
	}
	return editor::Localize(language, label);
}

ImVec4 GetMoistureStatusColor(FarmMoistureStatus status) noexcept {
	switch (status) {
	case FarmMoistureStatus::Dry:
		return { 1.0f, 0.34f, 0.22f, 1.0f };
	case FarmMoistureStatus::Low:
		return { 1.0f, 0.72f, 0.18f, 1.0f };
	case FarmMoistureStatus::Good:
		return { 0.34f, 0.78f, 1.0f, 1.0f };
	case FarmMoistureStatus::Invalid:
	default:
		return { 0.52f, 0.52f, 0.52f, 1.0f };
	}
}

void DrawObservedStatus(
	const char* label,
	bool observed,
	EditorLanguage language) {
	const ImVec4 color = observed
		? ImVec4(0.30f, 0.86f, 0.38f, 1.0f)
		: ImVec4(0.52f, 0.52f, 0.52f, 1.0f);
	ImGui::TextColored(
		color,
		"%s  %s",
		editor::Localize(language, observed ? "Observed" : "Not observed"),
		editor::Localize(language, label));
}

const char* GetFeedbackKindLabel(
	FarmFeedbackKind kind,
	EditorLanguage language) noexcept {
	const char* label = "None";
	switch (kind) {
	case FarmFeedbackKind::Harvest:
		label = "Harvest";
		break;
	case FarmFeedbackKind::Sale:
		label = "Sold";
		break;
	case FarmFeedbackKind::EmptySale:
		label = "Empty Sale";
		break;
	case FarmFeedbackKind::InputLocked:
		label = "Input Lock";
		break;
	case FarmFeedbackKind::Restarted:
		label = "Restart";
		break;
	case FarmFeedbackKind::SeedPurchased:
		label = "Seed Purchased";
		break;
	case FarmFeedbackKind::NoSeed:
		label = "No Seed";
		break;
	case FarmFeedbackKind::InsufficientMoney:
		label = "Insufficient Money";
		break;
	case FarmFeedbackKind::None:
	default:
		break;
	}
	return editor::Localize(language, label);
}

ImVec4 GetFeedbackKindColor(FarmFeedbackKind kind) noexcept {
	switch (kind) {
	case FarmFeedbackKind::EmptySale:
	case FarmFeedbackKind::InputLocked:
	case FarmFeedbackKind::NoSeed:
	case FarmFeedbackKind::InsufficientMoney:
		return { 1.0f, 0.32f, 0.22f, 1.0f };
	case FarmFeedbackKind::Harvest:
	case FarmFeedbackKind::Sale:
	case FarmFeedbackKind::Restarted:
	case FarmFeedbackKind::SeedPurchased:
		return { 0.30f, 0.86f, 0.38f, 1.0f };
	case FarmFeedbackKind::None:
	default:
		return { 0.52f, 0.52f, 0.52f, 1.0f };
	}
}

const char* GetQualityGrade(int score) noexcept
{
	if (score >= 90) {
		return "S";
	}
	if (score >= 75) {
		return "A";
	}
	if (score >= 60) {
		return "B";
	}
	if (score >= 40) {
		return "C";
	}
	return "D";
}

void DrawQualityRadar(
	const FarmCropQualityResult& quality,
	EditorLanguage language)
{
	if (!quality.IsValid()) {
		ImGui::TextDisabled("%s", editor::Localize(language, "No quality data"));
		return;
	}

	const float availableWidth = (std::max)(ImGui::GetContentRegionAvail().x, 180.0f);
	const float canvasWidth = (std::min)(availableWidth, 260.0f);
	constexpr float canvasHeight = 178.0f;
	constexpr float radius = 62.0f;
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 center = { origin.x + canvasWidth * 0.5f, origin.y + 76.0f };
	const std::array<ImVec2, 3> axes = {{
		{ 0.0f, -1.0f },
		{ 0.8660254f, 0.5f },
		{ -0.8660254f, 0.5f },
	}};
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.46f, 0.49f, 0.54f, 0.72f));
	for (int ring = 1; ring <= 4; ++ring) {
		const float ringRadius = radius * static_cast<float>(ring) / 4.0f;
		std::array<ImVec2, 4> points{};
		for (std::size_t axis = 0; axis < axes.size(); ++axis) {
			points[axis] = {
				center.x + axes[axis].x * ringRadius,
				center.y + axes[axis].y * ringRadius,
			};
		}
		points[3] = points[0];
		drawList->AddPolyline(
			points.data(), static_cast<int>(points.size()), gridColor, ImDrawFlags_None, 1.0f);
	}
	for (const ImVec2& axis : axes) {
		drawList->AddLine(
			center,
			{ center.x + axis.x * radius, center.y + axis.y * radius },
			gridColor,
			1.0f);
	}

	const std::array<float, 3> values = {{
		std::clamp(quality.maturity, 0.0f, 1.0f),
		std::clamp(quality.waterBalance, 0.0f, 1.0f),
		std::clamp(quality.terrainFit, 0.0f, 1.0f),
	}};
	std::array<ImVec2, 3> valuePoints{};
	for (std::size_t axis = 0; axis < axes.size(); ++axis) {
		valuePoints[axis] = {
			center.x + axes[axis].x * radius * values[axis],
			center.y + axes[axis].y * radius * values[axis],
		};
	}
	const ImU32 fillColor = ImGui::GetColorU32(ImVec4(0.96f, 0.67f, 0.12f, 0.28f));
	const ImU32 outlineColor = ImGui::GetColorU32(ImVec4(1.0f, 0.76f, 0.16f, 1.0f));
	drawList->AddConvexPolyFilled(valuePoints.data(), 3, fillColor);
	drawList->AddPolyline(valuePoints.data(), 3, outlineColor, ImDrawFlags_Closed, 2.0f);
	for (const ImVec2& point : valuePoints) {
		drawList->AddCircleFilled(point, 3.5f, outlineColor);
	}

	const char* maturityLabel = editor::Localize(language, "Maturity");
	const char* waterLabel = editor::Localize(language, "Water Balance");
	const char* terrainLabel = editor::Localize(language, "Terrain Fit");
	drawList->AddText(
		{ center.x - ImGui::CalcTextSize(maturityLabel).x * 0.5f, origin.y },
		ImGui::GetColorU32(ImGuiCol_Text), maturityLabel);
	drawList->AddText(
		{ center.x + radius * 0.55f, center.y + radius * 0.50f },
		ImGui::GetColorU32(ImGuiCol_Text), waterLabel);
	drawList->AddText(
		{ center.x - radius - ImGui::CalcTextSize(terrainLabel).x * 0.75f,
			center.y + radius * 0.50f },
		ImGui::GetColorU32(ImGuiCol_Text), terrainLabel);
	ImGui::Dummy({ canvasWidth, canvasHeight });

	ImGui::Text(
		editor::Localize(language, "Score: %d / 100  Grade %s"),
		quality.score,
		GetQualityGrade(quality.score));
	ImGui::Text(
		editor::Localize(language, "Estimated Value: %dG (Base %dG)"),
		quality.salePrice,
		quality.basePrice);
}
#endif
} // namespace

FarmControllerActions FarmControllerWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	EditorLanguage language)
{
	FarmControllerActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		{ viewport->WorkPos.x + (std::max)(viewport->WorkSize.x - 292.0f, 0.0f),
			viewport->WorkPos.y },
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 320.0f, 640.0f }, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(text("Farm Inspector###FarmInspector"), &open_, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return actions;
	}

	const int selectedIndex = viewModel.selectedFarmTileIndex;
	const editor::FarmTileEditorViewData* tile =
		selectedIndex >= 0 && selectedIndex < static_cast<int>(viewModel.farmTiles.size())
		? &viewModel.farmTiles[static_cast<std::size_t>(selectedIndex)]
		: nullptr;
	const editor::FarmPlaytestEditorViewData& playtest = viewModel.farmPlaytest;

	ImGui::SeparatorText(text("Playtest"));
	const ImVec4 stateColor = playtest.cleared
		? ImVec4(0.30f, 0.86f, 0.38f, 1.0f)
		: ImVec4(0.94f, 0.72f, 0.18f, 1.0f);
	ImGui::TextColored(
		stateColor,
		"%s",
		text(playtest.cleared ? "CLEARED" : "PLAYING"));
	ImGui::SameLine();
	ImGui::TextColored(
		playtest.inputLocked
			? ImVec4(1.0f, 0.32f, 0.22f, 1.0f)
			: ImVec4(0.30f, 0.86f, 0.38f, 1.0f),
		"%s",
		text(playtest.inputLocked ? "Input Locked" : "Input Enabled"));

	std::array<char, 64> progressOverlay{};
	std::snprintf(
		progressOverlay.data(),
		progressOverlay.size(),
		"%d / %d G",
		playtest.money,
		playtest.targetMoney);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, stateColor);
	ImGui::ProgressBar(
		std::clamp(playtest.progress, 0.0f, 1.0f),
		ImVec2(-1.0f, 0.0f),
		progressOverlay.data());
	ImGui::PopStyleColor();

	ImGui::Text(text("Inventory: %d crop(s)"), playtest.cropCount);
	ImGui::SameLine();
	ImGui::TextDisabled(text("Sale value: %dG"), playtest.saleValue);
	ImGui::Text(
		text("Selected seed: %s / %d"),
		farm::ToString(playtest.selectedSeedCrop),
		playtest.seedCount);
	ImGui::SameLine();
	ImGui::TextDisabled(text("Buy: B / %dG each"), playtest.seedPrice);
	ImGui::TextDisabled(text("Unit price: %dG"), playtest.cropSellPrice);
	if (ImGui::BeginTable(
		"CropInventory", 4,
		ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn(text("Crop"));
		ImGui::TableSetupColumn(text("Seeds"));
		ImGui::TableSetupColumn(text("Harvested"));
		ImGui::TableSetupColumn(text("Value"));
		ImGui::TableHeadersRow();
		for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
			const std::size_t index = static_cast<std::size_t>(slot);
			const farm::CropType crop = farm::CropTypeFromSlot(slot);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(text(farm::ToString(crop)));
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", playtest.seedCounts[index]);
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d", playtest.cropCounts[index]);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%dG", playtest.cropValues[index]);
		}
		ImGui::EndTable();
	}
	ImGui::TextDisabled("%s", text("F: Sell all / V: Sell selected crop"));
	if (!playtest.cleared) {
		if (playtest.requiredCropCount >= 0) {
			ImGui::Text(text("Goal: %dG left / %d crop(s) needed"),
				playtest.remainingMoney, playtest.requiredCropCount);
		} else {
			ImGui::TextColored(
				ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
				"%s",
				text("Goal cannot be calculated"));
		}
	}
	ImGui::TextUnformatted(text("Last Action"));
	ImGui::SameLine();
	ImGui::TextColored(
		GetFeedbackKindColor(playtest.lastFeedbackKind),
		"%s",
		playtest.feedbackMessage.empty()
			? GetFeedbackKindLabel(playtest.lastFeedbackKind, language)
			: playtest.feedbackMessage.c_str());

	if (ImGui::CollapsingHeader(text("Video Verification"))) {
		DrawObservedStatus("Empty Sale", playtest.feedbackStats.emptySaleCount > 0, language);
		DrawObservedStatus(
			"Clear",
			playtest.cleared || playtest.feedbackStats.goalReachedCount > 0,
			language);
		DrawObservedStatus("Input Lock", playtest.feedbackStats.inputLockedCount > 0, language);
		DrawObservedStatus("Seed Purchase", playtest.feedbackStats.seedPurchaseCount > 0, language);
		DrawObservedStatus("No Seed", playtest.feedbackStats.noSeedCount > 0, language);
		DrawObservedStatus(
			"Insufficient Money",
			playtest.feedbackStats.insufficientMoneyCount > 0,
			language);
		DrawObservedStatus("Restart", playtest.restartCount > 0, language);
	}
	if (ImGui::Button(text("Restart Farm"), ImVec2(-1.0f, 30.0f))) {
		actions.restartFarm = true;
	}
	ImGui::SeparatorText(text("Irrigation Network"));
	ImGui::Text(
		text("Sources %d / Supplied canals %d / Irrigation range %d"),
		viewModel.irrigationWaterSourceCount,
		viewModel.irrigationSuppliedCanalCount,
		viewModel.irrigationRangeTileCount);
	if (viewModel.irrigationPreviewActive) {
		ImGui::SeparatorText(text("Irrigation Preview"));
		if (viewModel.irrigationPathIssue == farm::FarmCanalPathIssue::NonStraight) {
			ImGui::TextWrapped("%s", text("Move along one row or column; turn at a tile"));
		} else if (viewModel.irrigationPathIssue == farm::FarmCanalPathIssue::BlockedTile) {
			ImGui::TextWrapped(text("Canal path blocked at tile #%d"), viewModel.irrigationBlockedTileIndex);
		}
		ImGui::TextWrapped("%s", text("Farm time paused during preview"));
		const bool canalRemovalPreview = viewModel.irrigationPreviewOperation ==
			farm::FarmIrrigationPreviewOperation::RemoveCanalPath;
		const bool canalPathPreview = canalRemovalPreview ||
			viewModel.irrigationPreviewOperation ==
				farm::FarmIrrigationPreviewOperation::PlaceCanalPath;
		const bool terrainPreview =
			viewModel.irrigationPreviewOperation ==
				farm::FarmIrrigationPreviewOperation::RaiseTerrain ||
			viewModel.irrigationPreviewOperation ==
				farm::FarmIrrigationPreviewOperation::LowerTerrain;
		if (canalPathPreview) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.76f, 0.18f, 1.0f),
				text(canalRemovalPreview ? "Canal removal: %d tiles" : "Canal path: %d tiles"),
				viewModel.irrigationPreviewChangeCount);
			ImGui::TextWrapped(
				"%s",
				text("Release the mouse, inspect the result, then confirm"));
		} else if (terrainPreview) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.76f, 0.18f, 1.0f),
				text("Tile #%d: H%d -> H%d"),
				viewModel.irrigationPreviewTileIndex,
				viewModel.irrigationPreviewOriginalHeightLevel,
				viewModel.irrigationPreviewCandidateHeightLevel);
		} else {
			ImGui::TextColored(
				ImVec4(1.0f, 0.76f, 0.18f, 1.0f),
				text("Tile #%d: %s -> %s"),
				viewModel.irrigationPreviewTileIndex,
				text(farm::ToString(viewModel.irrigationPreviewOriginalFeature)),
				text(farm::ToString(viewModel.irrigationPreviewCandidateFeature)));
		}
		ImGui::Text(
			text("Preview: Sources %d / Canals %d / Range %d"),
			viewModel.irrigationPreviewWaterSourceCount,
			viewModel.irrigationPreviewSuppliedCanalCount,
			viewModel.irrigationPreviewRangeTileCount);
		if (!viewModel.irrigationPreviewCanConfirm) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.34f, 0.20f, 1.0f),
				"%s",
				text("Farm changed. Cancel and preview again."));
		}
		const float actionWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		ImGui::BeginDisabled(!viewModel.irrigationPreviewCanConfirm);
		if (ImGui::Button(text("Confirm Preview"), { actionWidth, 30.0f })) {
			actions.confirmIrrigationPreview = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button(text("Cancel Preview"), { actionWidth, 30.0f })) {
			actions.cancelIrrigationPreview = true;
		}
	}

	ImGui::SeparatorText(text("Selection"));
	if (tile != nullptr) {
		ImGui::Text(text("Tile #%d"), tile->index);
		ImGui::SameLine();
		ImGui::TextDisabled(text("Coordinate (%d, %d)"), tile->column, tile->row);
		const ImVec4 stateColor = GetStateColor(*tile);
		ImGui::ColorButton(
			"##SelectedTileState",
			stateColor,
			ImGuiColorEditFlags_NoTooltip,
			{ 12.0f, 12.0f });
		ImGui::SameLine();
		ImGui::Text(text("State: %s"), GetStateLabel(*tile, language));
		ImGui::Text(text("Crop: %s"), text(farm::ToString(tile->crop)));

		ImGui::SeparatorText(text("Terrain"));
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(text("Height"));
		ImGui::SameLine();
		ImGui::BeginDisabled(
			tile->heightLevel <= FarmToolActionSystem::kMinimumHeightLevel ||
			viewModel.irrigationPreviewActive);
		if (ImGui::Button("-##Height", { 34.0f, 0.0f })) {
			actions.beginLowerTerrainPreview = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Preview lowering selected tile"));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Text("H%d", tile->heightLevel);
		ImGui::SameLine();
		ImGui::BeginDisabled(
			tile->heightLevel >= FarmToolActionSystem::kMaximumHeightLevel ||
			viewModel.irrigationPreviewActive);
		if (ImGui::Button("+##Height", { 34.0f, 0.0f })) {
			actions.beginRaiseTerrainPreview = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Preview raising selected tile"));
		}
		ImGui::EndDisabled();
		ImGui::Text(
			text("Feature: %s"),
			text(farm::ToString(tile->feature)));
		if (tile->feature != farm::FarmTileFeature::None) {
			char waterOverlay[64]{};
			std::snprintf(waterOverlay, sizeof(waterOverlay), text("Stored water: %.0f%%"),
				std::clamp(tile->storedWater, 0.0f, 1.0f) * 100.0f);
			ImGui::ProgressBar(std::clamp(tile->storedWater, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), waterOverlay);
			ImGui::TextWrapped("%s", text("Stored water remains after disconnection; removal discards it."));
			const float supplyStrength =
				std::clamp(tile->irrigationSupplyStrength, 0.0f, 1.0f);
			char supplyOverlay[64]{};
			std::snprintf(
				supplyOverlay,
				sizeof(supplyOverlay),
				text("Supply strength: %.0f%%"),
				supplyStrength * 100.0f);
			ImGui::PushStyleColor(
				ImGuiCol_PlotHistogram,
				ImVec4(0.16f, 0.76f, 0.95f, 1.0f));
			ImGui::ProgressBar(supplyStrength, ImVec2(-1.0f, 0.0f), supplyOverlay);
			ImGui::PopStyleColor();
			ImGui::TextDisabled(
				text("Downstream canals: %d"),
				tile->irrigationDownstreamCanalCount);
		}
		ImGui::BeginDisabled(
			!tile->canToggleCanal || viewModel.irrigationPreviewActive);
		const char* canalButtonLabel = tile->feature == farm::FarmTileFeature::Canal
			? "Preview Canal Removal"
			: "Preview Canal Placement";
		if (ImGui::Button(text(canalButtonLabel), ImVec2(-1.0f, 0.0f))) {
			actions.beginCanalPreview = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Preview canal change on selected tile"));
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(
			!tile->canToggleWaterSource || viewModel.irrigationPreviewActive);
		const char* sourceButtonLabel = tile->feature == farm::FarmTileFeature::WaterSource
			? "Preview Water Source Removal"
			: "Preview Water Source Placement";
		if (ImGui::Button(text(sourceButtonLabel), ImVec2(-1.0f, 0.0f))) {
			actions.beginWaterSourcePreview = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Preview water source change on selected tile"));
		}
		ImGui::EndDisabled();
		if (tile->feature == farm::FarmTileFeature::None) {
			ImGui::TextColored(
				tile->irrigationInRange
					? ImVec4(0.20f, 0.88f, 1.0f, 1.0f)
					: ImVec4(0.52f, 0.52f, 0.52f, 1.0f),
				text("Irrigation range: %s"),
				text(tile->irrigationInRange ? "In range" : "Out of range"));
			if (tile->irrigationSupplierTileIndex >= 0) {
				ImGui::TextDisabled(
					text("Supplied by canal #%d"),
					tile->irrigationSupplierTileIndex);
				ImGui::Text(
					text("Irrigation strength: %.0f%%"),
					std::clamp(tile->irrigationStrength, 0.0f, 1.0f) * 100.0f);
			}
		}

		ImGui::SeparatorText(text("Crop Status"));
		ImGui::Text("%s", text(farm::ToString(tile->waterStatus)));
		ImGui::TextWrapped("%s", text("Availability is not measured delivery. Soil receives water only after cultivation."));
		const float moisture = std::clamp(tile->moisture, 0.0f, 1.0f);
		const float growth = std::clamp(tile->growth, 0.0f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.25f, 0.60f, 0.95f, 1.0f));
		ImGui::ProgressBar(moisture, ImVec2(-1.0f, 0.0f), text("Water"));
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(
			ImGuiCol_PlotHistogram,
			tile->canHarvest
				? ImVec4(1.0f, 0.76f, 0.12f, 1.0f)
				: ImVec4(0.25f, 0.82f, 0.34f, 1.0f));
		ImGui::ProgressBar(growth, ImVec2(-1.0f, 0.0f), text("Growth"));
		ImGui::PopStyleColor();
		const FarmGrowthForecast& forecast = tile->growthForecast;
		if (tile->growthStage != farm::FarmCropGrowthStage::None) {
			ImGui::Text(
				text("Stage: %s"),
				GetStateLabel(*tile, language));
		}
		if (forecast.moistureValid) {
			ImGui::TextWrapped("%s", text("Recovery forecast is an upper bound while water remains; sharing reduces it."));
			ImGui::Text(
				text("Growth profile: %s"),
				text(farm::ToString(forecast.profileCrop)));
			ImGui::TextDisabled(
				text("Good moisture: %.0f%% or more"),
				forecast.goodMoistureMinimum * 100.0f);
			ImGui::TextColored(
				GetMoistureStatusColor(forecast.moistureStatus),
				text("Moisture: %s"),
				GetMoistureStatusLabel(forecast.moistureStatus, language));
			if (forecast.irrigationActive) {
				ImGui::TextColored(
					ImVec4(0.20f, 0.88f, 1.0f, 1.0f),
					"%s",
					text(farm::ToString(tile->waterStatus)));
				ImGui::Text(
					text("Irrigation recovery: %.1f%% / sec"),
					forecast.irrigationRecoveryPerSecond * 100.0f);
				ImGui::Text(
					text("Net moisture: %+.1f%% / sec"),
					forecast.netMoisturePerSecond * 100.0f);
				if (forecast.secondsUntilFullMoisture >= 0.0f) {
					ImGui::TextDisabled(
						text("Full moisture ETA: %.1f sec"),
						forecast.secondsUntilFullMoisture);
				}
			}
			if (forecast.growing) {
				ImGui::Text(
					text("Growth rate: %.1f%% / sec"),
					forecast.growthPerSecond * 100.0f);
				if (forecast.secondsUntilReady >= 0.0f) {
					ImGui::Text(
						text("Ready ETA: %.1f sec (current rate)"),
						forecast.secondsUntilReady);
				}
			}
			if (forecast.secondsUntilDry >= 0.0f) {
				ImGui::TextDisabled(
					text("Dry ETA: %.1f sec"),
					forecast.secondsUntilDry);
			}
		} else {
			ImGui::TextDisabled("%s", text("No active growth forecast"));
		}
		ImGui::TextDisabled("%s", GetNextActionLabel(*tile, language));
	} else {
		ImGui::TextDisabled("%s", text("None"));
	}

	ImGui::SeparatorText(text("Crop Quality"));
	const FarmCropQualityResult* quality = nullptr;
	if (tile != nullptr && tile->quality.IsValid()) {
		quality = &tile->quality;
		ImGui::TextDisabled("%s", text("Selected Tile Preview"));
	} else if (playtest.lastHarvestQuality.IsValid()) {
		quality = &playtest.lastHarvestQuality;
		ImGui::TextDisabled("%s", text("Last Harvest Result"));
	}
	if (quality != nullptr) {
		DrawQualityRadar(*quality, language);
	} else {
		ImGui::TextDisabled("%s", text("No quality data"));
	}

	if (ImGui::CollapsingHeader(text("History Integrity"))) {
		ImGui::Text(
			text("Undo: %zu / Redo: %zu"),
			viewModel.undoCount,
			viewModel.redoCount);
		ImGui::Text(
			text("Inventory: %d crop(s) / exact value %dG"),
			playtest.cropCount,
			playtest.saleValue);
		if (playtest.lastHarvestQuality.IsValid()) {
			ImGui::Text(
				text("Last harvest: %s / Q%d / %dG"),
				farm::ToString(playtest.lastHarvestQuality.crop),
				playtest.lastHarvestQuality.score,
				playtest.lastHarvestQuality.salePrice);
		} else {
			ImGui::TextDisabled("%s", text("Last harvest: None"));
		}
		ImGui::TextDisabled(
			"%s",
			text("Verify Harvest -> Undo -> Redo returns these values exactly."));
	}

	ImGui::SeparatorText(text("Farm Tool"));
	for (std::size_t index = 0; index < kToolButtons.size(); ++index) {
		const ToolButtonData& button = kToolButtons[index];
		const bool selected = button.tool == viewModel.currentFarmTool;
		if (selected) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.62f, 0.30f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.72f, 0.38f, 1.0f));
		}
		if (ImGui::Button(text(button.label), ImVec2(92.0f, 0.0f))) {
			actions.selectedTool = button.tool;
		}
		if (selected) {
			ImGui::PopStyleColor(2);
		}
		if (index + 1 < kToolButtons.size() && index % 2 == 0) {
			ImGui::SameLine();
		}
	}

	const bool canApply = tile != nullptr && viewModel.selectedFarmAction.Succeeded();
	const ImVec4 statusColor = canApply
		? ImVec4(0.25f, 0.95f, 0.38f, 1.0f)
		: ImVec4(1.0f, 0.32f, 0.20f, 1.0f);
	ImGui::TextColored(
		statusColor,
		"%s",
		GetActionStatusLabel(viewModel.selectedFarmAction.status, language));
	ImGui::BeginDisabled(!canApply);
	if (ImGui::Button(text("Apply Selected Tool"), ImVec2(-1.0f, 34.0f))) {
		actions.applyCurrentTool = true;
	}
	ImGui::EndDisabled();

	ImGui::SeparatorText(text("Viewport Shortcuts"));
	ImGui::TextDisabled("%s", text("Click: select   Shift+Click: select and apply"));
	ImGui::TextDisabled("%s", text("Arrows: move   Q/E: tool   Enter: apply"));
	ImGui::End();
#else
	(void)viewModel;
	(void)language;
#endif
	return actions;
}
