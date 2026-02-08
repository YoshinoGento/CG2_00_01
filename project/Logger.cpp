#include "Logger.h"
#include <windows.h> // OutputDebugStringA を使うため
#include <iostream>

namespace Logger {
    void Log(const std::string& message) {
        // Visual Studioの「出力」ウィンドウに出す
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");

        // 念のため標準出力にも出す
        std::cout << message << std::endl;
    }
}