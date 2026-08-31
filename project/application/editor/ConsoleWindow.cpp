#include "editor/ConsoleWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>

namespace {
bool ContainsSearchText(std::string_view text, std::string_view search) {
	if (search.empty()) {
		return true;
	}
	return std::search(
		text.begin(), text.end(), search.begin(), search.end(),
		[](char left, char right) {
			const unsigned char leftByte = static_cast<unsigned char>(left);
			const unsigned char rightByte = static_cast<unsigned char>(right);
			return leftByte < 0x80u && rightByte < 0x80u
				? std::tolower(leftByte) == std::tolower(rightByte)
				: leftByte == rightByte;
		}) != text.end();
}

ImVec4 GetLevelColor(Logger::Level level) noexcept {
	switch (level) {
	case Logger::Level::Warning:
		return { 1.0f, 0.72f, 0.18f, 1.0f };
	case Logger::Level::Error:
		return { 1.0f, 0.30f, 0.24f, 1.0f };
	case Logger::Level::Info:
	default:
		return { 0.48f, 0.76f, 1.0f, 1.0f };
	}
}

const char* GetLevelLabel(Logger::Level level, EditorLanguage language) noexcept {
	switch (level) {
	case Logger::Level::Warning:
		return editor::Localize(language, "Warning");
	case Logger::Level::Error:
		return editor::Localize(language, "Error");
	case Logger::Level::Info:
	default:
		return editor::Localize(language, "Info");
	}
}
} // namespace
#endif

ConsoleWindowActions ConsoleWindow::Draw(EditorLanguage language) {
	ConsoleWindowActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	const bool snapshotChanged =
		Logger::CopyEntriesIfChanged(loggerRevision_, cachedEntries_);
	if (snapshotChanged) {
		const bool selectionExists = std::any_of(
			cachedEntries_.begin(), cachedEntries_.end(),
			[&](const Logger::Entry& entry) { return entry.sequence == selectedSequence_; });
		if (!selectionExists) {
			selectedSequence_ = 0;
		}
	}

	if (!ImGui::Begin(text("Console###Console"), &open_, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return actions;
	}

	std::size_t infoCount = 0;
	std::size_t warningCount = 0;
	std::size_t errorCount = 0;
	for (const Logger::Entry& entry : cachedEntries_) {
		switch (entry.level) {
		case Logger::Level::Warning:
			++warningCount;
			break;
		case Logger::Level::Error:
			++errorCount;
			break;
		case Logger::Level::Info:
		default:
			++infoCount;
			break;
		}
	}

	bool filterChanged = snapshotChanged;
	if (ImGui::Button(text("Clear"), { 72.0f, 0.0f })) {
		actions.clearRequested = true;
		selectedSequence_ = 0;
	}
	ImGui::SameLine();
	char label[64]{};
	std::snprintf(label, sizeof(label), "%s %zu###ConsoleInfoFilter", text("Info"), infoCount);
	if (ImGui::Checkbox(label, &showInfo_)) {
		filterChanged = true;
	}
	ImGui::SameLine();
	std::snprintf(label, sizeof(label), "%s %zu###ConsoleWarningFilter", text("Warning"), warningCount);
	if (ImGui::Checkbox(label, &showWarning_)) {
		filterChanged = true;
	}
	ImGui::SameLine();
	std::snprintf(label, sizeof(label), "%s %zu###ConsoleErrorFilter", text("Error"), errorCount);
	if (ImGui::Checkbox(label, &showError_)) {
		filterChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(260.0f);
	if (ImGui::InputTextWithHint(
		"##ConsoleSearch",
		text("Search logs..."),
		searchBuffer_.data(),
		searchBuffer_.size())) {
		filterChanged = true;
	}
	ImGui::SameLine();
	ImGui::Checkbox(text("Auto-scroll"), &autoScroll_);

	if (filterChanged) {
		RebuildVisibleEntries();
	}

	const float detailHeight = selectedSequence_ != 0 ? 58.0f : 0.0f;
	const float tableHeight = (std::max)(ImGui::GetContentRegionAvail().y - detailHeight, 80.0f);
	constexpr ImGuiTableFlags kTableFlags =
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("ConsoleEntries", 3, kTableFlags, { 0.0f, tableHeight })) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(text("Time"), ImGuiTableColumnFlags_WidthFixed, 72.0f);
		ImGui::TableSetupColumn(text("Level"), ImGuiTableColumnFlags_WidthFixed, 86.0f);
		ImGui::TableSetupColumn(text("Message"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(visibleEntryIndices_.size()));
		while (clipper.Step()) {
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
				const Logger::Entry& entry =
					cachedEntries_[visibleEntryIndices_[static_cast<std::size_t>(row)]];
				ImGui::PushID(static_cast<int>(entry.sequence & 0x7fffffffu));
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				char timeLabel[32]{};
				std::snprintf(timeLabel, sizeof(timeLabel), "%7.2fs", entry.secondsSinceStart);
				if (ImGui::Selectable(
					timeLabel,
					selectedSequence_ == entry.sequence,
					ImGuiSelectableFlags_SpanAllColumns)) {
					selectedSequence_ = entry.sequence;
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored(GetLevelColor(entry.level), "%s", GetLevelLabel(entry.level, language));
				if (entry.repeatCount > 1) {
					ImGui::SameLine();
					ImGui::TextDisabled("x%u", entry.repeatCount);
				}
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(entry.message.c_str());
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", entry.message.c_str());
				}
				ImGui::PopID();
			}
		}
		if (visibleEntryIndices_.empty()) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(2);
			ImGui::TextDisabled("%s", text("No logs match the current filters."));
		}
		if (snapshotChanged && autoScroll_) {
			ImGui::SetScrollY(ImGui::GetScrollMaxY());
		}
		ImGui::EndTable();
	}

	if (selectedSequence_ != 0) {
		const auto selected = std::find_if(
			cachedEntries_.begin(), cachedEntries_.end(),
			[&](const Logger::Entry& entry) { return entry.sequence == selectedSequence_; });
		if (selected != cachedEntries_.end()) {
			ImGui::SeparatorText(text("Selected log"));
			ImGui::TextWrapped("%s", selected->message.c_str());
		}
	}
	ImGui::End();
#else
	(void)language;
#endif
	return actions;
}

void ConsoleWindow::RebuildVisibleEntries() {
#ifdef USE_IMGUI
	visibleEntryIndices_.clear();
	visibleEntryIndices_.reserve(cachedEntries_.size());
	for (std::size_t index = 0; index < cachedEntries_.size(); ++index) {
		if (PassesFilter(cachedEntries_[index])) {
			visibleEntryIndices_.push_back(index);
		}
	}
#endif
}

bool ConsoleWindow::PassesFilter(const Logger::Entry& entry) const {
#ifdef USE_IMGUI
	const bool levelVisible =
		(entry.level == Logger::Level::Info && showInfo_) ||
		(entry.level == Logger::Level::Warning && showWarning_) ||
		(entry.level == Logger::Level::Error && showError_);
	return levelVisible && ContainsSearchText(entry.message, searchBuffer_.data());
#else
	(void)entry;
	return false;
#endif
}
