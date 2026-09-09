#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace farmui {
enum class RecordDialogMode { Name, LayoutLibrary, ProgressPicker };
enum class RecordDialogAction { Cancel, SaveNew, Load };
struct RecordDialogResult {
    RecordDialogAction action = RecordDialogAction::Cancel;
    std::string name;
    int selected = -1;
};
// Native Unicode/IME presentation only. The caller owns persistence and confirmation policy.
RecordDialogResult ShowRecordDialog(HWND owner, RecordDialogMode mode,
    const std::vector<std::string>& names, const std::string& initialName = {});
void RecordNotice(HWND owner, const std::string& message);
bool ConfirmLayoutReplacement(HWND owner);
}
