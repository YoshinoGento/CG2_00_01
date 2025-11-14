#pragma once
#include <string>

namespace StringUtility {

    // string -> wstring 変換
    std::wstring ConvertString(const std::string& str);

    // wstring -> string 変換
    std::string ConvertString(const std::wstring& str);
}
