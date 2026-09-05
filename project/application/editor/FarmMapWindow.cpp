#include "editor/FarmMapWindow.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#endif

namespace {
#ifdef USE_IMGUI
enum class FarmMapVisualState {
	Empty,
	CanalDry,
	CanalSupplied,
	CanalRetained,
	CanalWaiting,
	WaterSource,
	Tilled,
	Watered,
	Growing,
	Ready,
};

FarmMapVisualState GetVisualState(const editor::FarmTileEditorViewData& tile) noexcept {
	if (tile.feature == farm::FarmTileFeature::Canal) {
		switch (tile.waterStatus) {
		case farm::FarmWaterStatus::Available: return FarmMapVisualState::CanalSupplied;
		case farm::FarmWaterStatus::Retained: return FarmMapVisualState::CanalRetained;
		case farm::FarmWaterStatus::Waiting: return FarmMapVisualState::CanalWaiting;
		default: return FarmMapVisualState::CanalDry;
		}
	}
	if (tile.feature == farm::FarmTileFeature::WaterSource) {
		return FarmMapVisualState::WaterSource;
	}
	if (tile.canHarvest) {
		return FarmMapVisualState::Ready;
	}
	if (tile.state == farm::FarmTileState::Planted) {
		return FarmMapVisualState::Growing;
	}
	if (tile.state == farm::FarmTileState::Tilled) {
		return tile.moisture > 0.5f
			? FarmMapVisualState::Watered
			: FarmMapVisualState::Tilled;
	}
	return FarmMapVisualState::Empty;
}

ImVec4 GetStateColor(FarmMapVisualState state) noexcept {
	switch (state) {
	case FarmMapVisualState::CanalDry: return { 0.10f, 0.30f, 0.48f, 1.0f };
	case FarmMapVisualState::CanalRetained: return { 0.65f, 0.44f, 0.12f, 1.0f };
	case FarmMapVisualState::CanalWaiting: return { 0.36f, 0.39f, 0.44f, 1.0f };
	case FarmMapVisualState::CanalSupplied: return { 0.08f, 0.72f, 0.98f, 1.0f };
	case FarmMapVisualState::WaterSource: return { 0.14f, 0.90f, 1.0f, 1.0f };
	case FarmMapVisualState::Tilled: return { 0.68f, 0.43f, 0.22f, 1.0f };
	case FarmMapVisualState::Watered: return { 0.30f, 0.52f, 0.72f, 1.0f };
	case FarmMapVisualState::Growing: return { 0.28f, 0.68f, 0.30f, 1.0f };
	case FarmMapVisualState::Ready: return { 0.95f, 0.72f, 0.16f, 1.0f };
	case FarmMapVisualState::Empty:
	default: return { 0.34f, 0.58f, 0.28f, 1.0f };
	}
}

const char* GetStateName(FarmMapVisualState state, EditorLanguage language) noexcept {
	const char* stateName = "Empty";
	switch (state) {
	case FarmMapVisualState::CanalDry: stateName = "Dry"; break;
	case FarmMapVisualState::CanalSupplied: stateName = "Wet"; break;
	case FarmMapVisualState::CanalRetained: stateName = "Stored"; break;
	case FarmMapVisualState::CanalWaiting: stateName = "Waiting"; break;
	case FarmMapVisualState::WaterSource: stateName = "Water Source"; break;
	case FarmMapVisualState::Tilled: stateName = "Tilled"; break;
	case FarmMapVisualState::Watered: stateName = "Watered"; break;
	case FarmMapVisualState::Growing: stateName = "Growing"; break;
	case FarmMapVisualState::Ready: stateName = "Ready"; break;
	case FarmMapVisualState::Empty:
	default: break;
	}
	return editor::Localize(language, stateName);
}

ImVec4 Brighten(const ImVec4& color, float amount) noexcept {
	return {
		(std::min)(color.x + amount, 1.0f),
		(std::min)(color.y + amount, 1.0f),
		(std::min)(color.z + amount, 1.0f),
		color.w,
	};
}

void DrawLegendItem(const char* label, const ImVec4& color) {
	ImGui::ColorButton(label, color, ImGuiColorEditFlags_NoTooltip, { 10.0f, 10.0f });
	ImGui::SameLine(0.0f, 4.0f);
	ImGui::TextDisabled("%s", label);
}
#endif
} // namespace

FarmMapActions FarmMapWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	const editor::EditorSelection& selection,
	EditorLanguage language)
{
	FarmMapActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	if (!ImGui::Begin(text("Farm Map###FarmMap"), &open_, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return actions;
	}

	if (viewModel.farmWidth <= 0 || viewModel.farmHeight <= 0 || viewModel.farmTiles.empty()) {
		ImGui::TextDisabled("%s", text("Farm data is unavailable."));
		ImGui::End();
		return actions;
	}

	ImGui::Text(text("Farm Grid  %d x %d"), viewModel.farmWidth, viewModel.farmHeight);
	const bool canalPathPreviewActive =
		viewModel.irrigationPreviewActive &&
		(viewModel.irrigationPreviewOperation == farm::FarmIrrigationPreviewOperation::PlaceCanalPath ||
		 viewModel.irrigationPreviewOperation == farm::FarmIrrigationPreviewOperation::RemoveCanalPath);
	if (viewModel.irrigationPreviewActive) {
		ImGui::SameLine();
		ImGui::TextColored(
			ImVec4(1.0f, 0.76f, 0.18f, 1.0f),
			"%s",
			text("Irrigation Preview"));
	}
	const bool brushBlocked = viewModel.irrigationPreviewActive && !canalPathPreviewActive;
	ImGui::BeginDisabled(brushBlocked);
	if (ImGui::Checkbox(text("Canal Brush"), &canalBrushEnabled_) && !canalBrushEnabled_) {
		canalDragActive_ = false;
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", text("Drag across adjacent empty tiles to preview a canal path"));
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		canalDragActive_ = false;
		lastDragTileIndex_ = -1;
	}
	if (canalPathPreviewActive) {
		canalEraseMode_ = viewModel.irrigationPreviewOperation ==
			farm::FarmIrrigationPreviewOperation::RemoveCanalPath;
	}
	if (canalBrushEnabled_) {
		ImGui::BeginDisabled(viewModel.irrigationPreviewActive);
		if (ImGui::RadioButton(text("Place canals"), !canalEraseMode_)) canalEraseMode_ = false;
		ImGui::SameLine();
		if (ImGui::RadioButton(text("Erase canals"), canalEraseMode_)) canalEraseMode_ = true;
		ImGui::EndDisabled();
		ImGui::TextWrapped("%s", text(canalEraseMode_
			? "Drag over canals; return along the path to shorten it"
			: "Drag over empty tiles; return along the path to shorten it"));
	} else {
		ImGui::TextDisabled("%s", text("Click a tile to select it"));
	}
	ImGui::Spacing();

	if (ImGui::BeginTable(
		"FarmMapGrid",
		viewModel.farmWidth,
		ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
		for (const editor::FarmTileEditorViewData& tile : viewModel.farmTiles) {
			ImGui::TableNextColumn();
			const bool selected =
				tile.index == viewModel.selectedFarmTileIndex ||
				(selection.type == editor::EditorSelectionType::FarmTile &&
				 selection.generation == viewModel.farmGeneration &&
				 selection.index == tile.index);
			const FarmMapVisualState visualState = GetVisualState(tile);
			const ImVec4 tileColor = GetStateColor(visualState);

			ImGui::PushID(tile.index);
			ImGui::PushStyleColor(ImGuiCol_Button, tileColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(tileColor, 0.12f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, Brighten(tileColor, 0.05f));
			ImGui::PushStyleColor(
				ImGuiCol_Border,
				tile.irrigationPreviewChanged
					? ImVec4(1.0f, 0.56f, 0.12f, 1.0f)
					: selected
					? ImVec4(1.0f, 0.90f, 0.10f, 1.0f)
					: ImVec4(0.0f, 0.0f, 0.0f, 0.40f));
			ImGui::PushStyleVar(
				ImGuiStyleVar_FrameBorderSize,
				tile.irrigationPreviewChanged || selected ? 3.0f : 1.0f);

			char label[48]{};
			std::snprintf(
				label,
				sizeof(label),
				"%02d\n%s\nH%d",
				tile.index,
				GetStateName(visualState, language),
				tile.heightLevel);
			const bool tilePressed = ImGui::Button(label, { -1.0f, 58.0f });
			const bool tileHovered = ImGui::IsItemHovered(canalDragActive_
				? ImGuiHoveredFlags_AllowWhenBlockedByActiveItem : ImGuiHoveredFlags_None);
			const bool tileActivated = ImGui::IsItemActivated();
			if (canalBrushEnabled_ && !brushBlocked) {
				if (tileActivated) {
					canalDragActive_ = true;
					lastDragTileIndex_ = -1;
					if (!viewModel.irrigationPreviewActive) {
						actions.beginCanalPathTileIndex = tile.index;
						actions.removeCanalPath = canalEraseMode_;
						lastDragTileIndex_ = tile.index;
					}
				}
				if (canalDragActive_ && canalPathPreviewActive && tileHovered &&
					ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
					lastDragTileIndex_ != tile.index) {
					actions.appendCanalPathTileIndex = tile.index;
					lastDragTileIndex_ = tile.index;
				}
			} else if (tilePressed) {
				actions.selectedTileIndex = tile.index;
			}

			const ImVec2 rectMin = ImGui::GetItemRectMin();
			const ImVec2 rectMax = ImGui::GetItemRectMax();
			const float barLeft = rectMin.x + 3.0f;
			const float barWidth = (std::max)(rectMax.x - rectMin.x - 6.0f, 0.0f);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const float moisture = std::clamp(tile.feature == farm::FarmTileFeature::None
				? tile.moisture : tile.storedWater, 0.0f, 1.0f);
			const float growth = std::clamp(tile.growth, 0.0f, 1.0f);
			drawList->AddRectFilled(
				{ barLeft, rectMax.y - 4.0f },
				{ barLeft + barWidth * moisture, rectMax.y - 2.0f },
				IM_COL32(70, 170, 255, 255));
			drawList->AddRectFilled(
				{ barLeft, rectMax.y - 7.0f },
				{ barLeft + barWidth * growth, rectMax.y - 5.0f },
				visualState == FarmMapVisualState::Ready
					? IM_COL32(255, 205, 45, 255)
					: IM_COL32(75, 220, 105, 255));
			const float irrigationBarStrength =
				tile.feature == farm::FarmTileFeature::None
				? std::clamp(tile.irrigationStrength, 0.0f, 1.0f)
				: std::clamp(tile.irrigationSupplyStrength, 0.0f, 1.0f);
			if (irrigationBarStrength > 0.0f) {
				drawList->AddRectFilled(
					{ barLeft, rectMin.y + 2.0f },
					{ barLeft + barWidth * irrigationBarStrength, rectMin.y + 5.0f },
					IM_COL32(45, 225, 255, 255));
			}

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text(text("Tile %d  (%d, %d)"), tile.index, tile.column, tile.row);
				ImGui::Text(text("State: %s"), GetStateName(visualState, language));
				ImGui::Text(text("Height: H%d"), tile.heightLevel);
				ImGui::Text("%s", text(farm::ToString(tile.waterStatus)));
				ImGui::Text(text(tile.feature == farm::FarmTileFeature::None
					? "Moisture: %.0f%%" : "Stored water: %.0f%%"), moisture * 100.0f);
				ImGui::Text(text("Growth: %.0f%%"), growth * 100.0f);
				if (tile.irrigationPreviewChanged) {
					ImGui::TextColored(
						ImVec4(1.0f, 0.76f, 0.18f, 1.0f),
						"%s",
						text("Preview changed tile"));
				}
				ImGui::Text(
					text("Irrigation range: %s"),
					text(tile.irrigationInRange ? "In range" : "Out of range"));
				if (tile.feature != farm::FarmTileFeature::None) {
					ImGui::Text(
						text("Supply strength: %.0f%%"),
						tile.irrigationSupplyStrength * 100.0f);
					ImGui::Text(
						text("Downstream canals: %d"),
						tile.irrigationDownstreamCanalCount);
				} else if (tile.irrigationInRange) {
					ImGui::Text(
						text("Irrigation strength: %.0f%%"),
						tile.irrigationStrength * 100.0f);
				}
				if (tile.irrigationSupplierTileIndex >= 0) {
					ImGui::Text(
						text("Supplied by canal #%d"),
						tile.irrigationSupplierTileIndex);
				}
				ImGui::EndTooltip();
			}

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(4);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (canalBrushEnabled_) {
		ImGui::TextWrapped("%s", text("Straight drags fill skipped tiles"));
	}
	if (viewModel.irrigationPathIssue != farm::FarmCanalPathIssue::None) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.60f, 0.28f, 1.0f));
		if (viewModel.irrigationPathIssue == farm::FarmCanalPathIssue::NonStraight) {
			ImGui::TextWrapped("%s", text("Move along one row or column; turn at a tile"));
		} else {
			ImGui::TextWrapped(text("Canal path blocked at tile #%d"), viewModel.irrigationBlockedTileIndex);
		}
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::SeparatorText(text("Legend"));
	DrawLegendItem(text("Empty"), GetStateColor(FarmMapVisualState::Empty));
	DrawLegendItem(text("Dry Canal"), GetStateColor(FarmMapVisualState::CanalDry));
	DrawLegendItem(text("Supplied"), GetStateColor(FarmMapVisualState::CanalSupplied));
	DrawLegendItem(text("Water Source"), GetStateColor(FarmMapVisualState::WaterSource));
	DrawLegendItem(text("Tilled"), GetStateColor(FarmMapVisualState::Tilled));
	DrawLegendItem(text("Watered"), GetStateColor(FarmMapVisualState::Watered));
	DrawLegendItem(text("Growing"), GetStateColor(FarmMapVisualState::Growing));
	DrawLegendItem(text("Ready"), GetStateColor(FarmMapVisualState::Ready));
	ImGui::TextWrapped("%s", text("Blue bar: soil moisture / stored canal water"));
	DrawLegendItem(text("Water available"), GetStateColor(FarmMapVisualState::CanalSupplied));
	DrawLegendItem(text("Retained water"), GetStateColor(FarmMapVisualState::CanalRetained));
	DrawLegendItem(text("Waiting for water"), GetStateColor(FarmMapVisualState::CanalWaiting));
	ImGui::TextDisabled("%s", text("Green/gold bar: growth"));
	ImGui::TextDisabled("%s", text("Cyan line: supply strength"));
	ImGui::End();
#else
	(void)viewModel;
	(void)selection;
	(void)language;
#endif
	return actions;
}
