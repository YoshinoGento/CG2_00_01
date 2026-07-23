#pragma once

#include <string_view>

enum class EditorLanguage {
	Japanese,
	English,
};

namespace editor {

// English source text is the stable key. Unknown keys intentionally fall back to English.
[[nodiscard]] const char* Localize(EditorLanguage language, std::string_view englishText) noexcept;
[[nodiscard]] const char* GetEditorLanguageStorageName(EditorLanguage language) noexcept;
[[nodiscard]] EditorLanguage ParseEditorLanguage(std::string_view value) noexcept;

} // namespace editor
