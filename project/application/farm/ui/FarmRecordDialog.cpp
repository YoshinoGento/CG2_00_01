#include "farm/ui/FarmRecordDialog.h"
#include <array>

namespace farmui {
namespace {
constexpr int kName = 100, kList = 101, kLoad = 102;
struct Context {
    RecordDialogMode mode;
    const std::vector<std::string>& names;
    std::string initialName;
    RecordDialogResult result;
    HFONT font = nullptr;
};
std::wstring Wide(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
std::string Utf8(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result.data(), size, nullptr, nullptr);
    result.pop_back(); return result;
}
HWND Control(HWND parent, Context& state, const wchar_t* type, const wchar_t* label,
    DWORD style, int id, int x, int y, int width, int height) {
    RECT r{x, y, x + width, y + height}; MapDialogRect(parent, &r);
    HWND child = CreateWindowExW(type == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
        type, label, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right-r.left, r.bottom-r.top,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (child) SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), TRUE);
    return child;
}
INT_PTR CALLBACK Procedure(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<Context*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<Context*>(lParam); SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        const bool naming = state->mode == RecordDialogMode::Name;
        const bool library = state->mode == RecordDialogMode::LayoutLibrary;
        SetWindowTextW(dialog, library ? L"畑の配置ライブラリ" : naming ? L"進行セーブに名前を付ける" : L"進行セーブを選ぶ");
        state->font = CreateFontW(-MulDiv(12, GetDpiForWindow(dialog), 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
        Control(dialog, *state, L"STATIC", library ? L"高さ・耕した区画・用水路・水源のみ。進行セーブとは別の記録です。"
            : naming ? L"保存名（日本語も入力できます）" : L"名前から記録を選択してください。", 0, 0, 12, 10, 416, 26);
        if (library || naming) {
            HWND edit = Control(dialog, *state, L"EDIT", Wide(state->initialName).c_str(), WS_TABSTOP | ES_AUTOHSCROLL, kName, 12, 42, 412, 24);
            SendMessageW(edit, EM_SETLIMITTEXT, 64, 0);
            SendMessageW(edit, EM_SETSEL, 0, -1);
        }
        if (!naming) {
            HWND list = Control(dialog, *state, L"LISTBOX", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                kList, 12, library ? 76 : 42, 412, library ? 108 : 142);
            for (const auto& name : state->names) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(name).c_str()));
            SendMessageW(list, LB_SETHORIZONTALEXTENT, 1100, 0);
            if (!state->names.empty()) SendMessageW(list, LB_SETCURSEL, 0, 0);
        }
        if (library || naming) Control(dialog, *state, L"BUTTON", library ? L"この名前で配置を保存" : L"保存", WS_TABSTOP | BS_DEFPUSHBUTTON, IDOK, 12, 202, 145, 28);
        if (!naming) {
            HWND load = Control(dialog, *state, L"BUTTON", library ? L"選んだ配置を適用…" : L"選んだ記録を開く…", WS_TABSTOP, kLoad, 168, 202, 145, 28);
            EnableWindow(load, !state->names.empty());
        }
        Control(dialog, *state, L"BUTTON", L"閉じる", WS_TABSTOP, IDCANCEL, 326, 202, 98, 28);
        RECT owner{}, bounds{}; GetWindowRect(GetParent(dialog), &owner); GetWindowRect(dialog, &bounds);
        const int width = bounds.right-bounds.left, height = bounds.bottom-bounds.top;
        SetWindowPos(dialog, HWND_TOP, owner.left + ((owner.right-owner.left)-width)/2,
            owner.top + ((owner.bottom-owner.top)-height)/2, 0, 0, SWP_NOSIZE);
        SetFocus(GetDlgItem(dialog, naming || library ? kName : kList)); return FALSE;
    }
    if (!state) return FALSE;
    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
        if (id == IDOK && state->mode != RecordDialogMode::ProgressPicker) {
            std::array<wchar_t, 66> value{}; GetDlgItemTextW(dialog, kName, value.data(), static_cast<int>(value.size()));
            const auto name = Utf8(value.data());
            if (name.empty() || name.size() > 192) {
                MessageBoxW(dialog, L"名前を入力してください（日本語は64文字まで）。", L"保存名", MB_OK | MB_ICONWARNING); return TRUE;
            }
            state->result = {RecordDialogAction::SaveNew, name, -1}; EndDialog(dialog, IDOK); return TRUE;
        }
        if (id == kLoad) {
            const LRESULT selected = SendDlgItemMessageW(dialog, kList, LB_GETCURSEL, 0, 0);
            if (selected >= 0 && static_cast<std::size_t>(selected) < state->names.size()) {
                state->result = {RecordDialogAction::Load, {}, static_cast<int>(selected)};
                EndDialog(dialog, IDOK);
            }
            return TRUE;
        }
    }
    if (message == WM_CLOSE) { EndDialog(dialog, IDCANCEL); return TRUE; }
    return FALSE;
}
}
RecordDialogResult ShowRecordDialog(HWND owner, RecordDialogMode mode,
    const std::vector<std::string>& names, const std::string& initialName) {
    // Zero strings following DLGTEMPLATE encode no menu, default class, empty title.
    struct Template { DLGTEMPLATE dialog; WORD menu = 0, windowClass = 0, title = 0; } model{};
    model.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    model.dialog.cx = 438; model.dialog.cy = 242;
    Context state{mode, names, initialName, {}, nullptr};
    const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &model.dialog, owner, Procedure, reinterpret_cast<LPARAM>(&state));
    if (state.font) DeleteObject(state.font);
    if (result == -1) RecordNotice(owner, "記録ウィンドウを開けませんでした。");
    return state.result;
}
void RecordNotice(HWND owner, const std::string& message) {
    MessageBoxW(owner, Wide(message).c_str(), L"農場の記録", MB_OK | MB_ICONINFORMATION);
}
bool ConfirmLayoutReplacement(HWND owner) {
    return MessageBoxW(owner,
        L"選んだ配置で畑全体を置き換えます。\n植わっている作物・成長度・水分は消去されます。\n所持金・持ち物・日数は変更しません。\n\n適用前の編集履歴はクリアされます。進行セーブは上書きしません。\n続けますか？",
        L"配置を適用", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDYES;
}
}
